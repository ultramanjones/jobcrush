#include "GeminiApiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <memory>

#include "AiBrainReply.h"

namespace {

const QString geminiApiBaseUrl =
    QStringLiteral("https://generativelanguage.googleapis.com/v1beta");

// The model for each brain level. Checked 2026-10-23: the old default,
// gemini-2.5-flash, is only open to projects that used it before, and is
// being shut down. These three are listed as stable, with no shutdown before
// mid-2027 for the first two. gemini-3.8-flash is newer and cheaper than
// gemini-3.5-flash, but Google marks it "short-term", so Balanced uses the
// one that will last.
QString geminiModelNameFor(AiBrainLevel brainLevel)
{
    switch (brainLevel) {
    case AiBrainLevel::Quick:    return QStringLiteral("gemini-3.5-flash-lite");
    case AiBrainLevel::Balanced: return QStringLiteral("gemini-3.5-flash");
    case AiBrainLevel::Best:     return QStringLiteral("gemini-4-argon");
    }
    return QStringLiteral("gemini-3.5-flash");
}

// Generous but bounded — chat answers and cover-letter drafts, not novels.
constexpr int geminiMaxResponseTokens = 2048;

// Each payload line in Google's stream starts with this.
const QByteArray serverSentEventDataPrefix = QByteArrayLiteral("data:");

// The HTTP answers Google gives for a key problem. Google sends 400 for a
// bad key, where most vendors send 401. 403 means the key is real but not
// allowed to use this API.
constexpr int httpBadRequest = 400;
constexpr int httpUnauthorized = 401;
constexpr int httpForbidden = 403;

// The HTTP answer for a model name Google does not have.
constexpr int httpNotFound = 404;

// True when Google's HTTP answer means the key was the problem.
bool isKeyProblemStatus(int httpStatusCode)
{
    return httpStatusCode == httpBadRequest
           || httpStatusCode == httpUnauthorized
           || httpStatusCode == httpForbidden;
}

// Google reads the key from this header. Their own samples put it in the URL
// instead, which is how API keys end up in server logs and screenshots.
const QByteArray geminiApiKeyHeaderName = QByteArrayLiteral("x-goog-api-key");

} // namespace

GeminiApiProvider::GeminiApiProvider() = default;

QString GeminiApiProvider::modelNameFor(AiBrainLevel brainLevel) const
{
    return geminiModelNameFor(brainLevel);
}

QString GeminiApiProvider::providerDisplayName() const
{
    return QStringLiteral("Gemini");
}

QByteArray GeminiApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiBrainRequestOptions &requestOptions) const
{
    // Google's shape: every turn is an object with a role and a list of
    // "parts". The soul goes in systemInstruction rather than in the turns,
    // which is closer to Anthropic's arrangement than OpenAI's.
    QJsonArray contentsArray;
    for (const AiBrainConversationMessage &conversationMessage : conversation) {
        QJsonObject partObject;
        partObject.insert(QStringLiteral("text"), conversationMessage.messageText);

        QJsonArray partsArray;
        partsArray.append(partObject);

        QJsonObject turnObject;
        // Google calls the assistant "model", not "assistant".
        turnObject.insert(QStringLiteral("role"),
                          conversationMessage.author == AiBrainConversationMessage::Author::Human
                              ? QStringLiteral("user")
                              : QStringLiteral("model"));
        turnObject.insert(QStringLiteral("parts"), partsArray);
        contentsArray.append(turnObject);
    }

    QJsonObject requestBodyObject;
    requestBodyObject.insert(QStringLiteral("contents"), contentsArray);

    if (!soulText.isEmpty()) {
        QJsonObject systemPartObject;
        systemPartObject.insert(QStringLiteral("text"), soulText);
        QJsonArray systemPartsArray;
        systemPartsArray.append(systemPartObject);

        QJsonObject systemInstructionObject;
        systemInstructionObject.insert(QStringLiteral("parts"), systemPartsArray);
        requestBodyObject.insert(QStringLiteral("systemInstruction"), systemInstructionObject);
    }

    QJsonObject generationConfigObject;
    generationConfigObject.insert(QStringLiteral("maxOutputTokens"),
                                  qMax(geminiMaxResponseTokens, requestOptions.longestAnswerTokens));
    requestBodyObject.insert(QStringLiteral("generationConfig"), generationConfigObject);

    // Google's own web search, which they call grounding. The model searches
    // on their side and the answer still comes back as text parts, so the
    // stream parser needs no change.
    if (requestOptions.letTheBrainSearchTheWeb) {
        QJsonObject googleSearchTool;
        googleSearchTool.insert(QStringLiteral("google_search"), QJsonObject());
        QJsonArray toolsArray;
        toolsArray.append(googleSearchTool);
        requestBodyObject.insert(QStringLiteral("tools"), toolsArray);
    }

    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *GeminiApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent,
    const AiBrainRequestOptions &requestOptions)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    // alt=sse is what turns this into a stream. Without it Google sends one
    // enormous JSON array at the end, and the chat would sit silent until the
    // whole answer existed — which is the frozen-looking wait the no-spinner
    // law exists to prevent.
    const QString modelName = geminiModelNameFor(requestOptions.brainLevel);
    const QUrl requestUrl(QStringLiteral("%1/models/%2:streamGenerateContent?alt=sse")
                              .arg(geminiApiBaseUrl, modelName));

    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(geminiApiKeyHeaderName, credential.secretKey.toUtf8());

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation, requestOptions));
    networkReply->setParent(brainReply); // dies with the brain reply

    // --- Google's server-sent events -------------------------------------
    //
    //     data: {"candidates":[{"content":{"parts":[{"text":"Hel"}]}}]}
    //
    // A rolling buffer, the same as OpenAiStyleStream uses. The steps:
    //  1. Add whatever bytes arrived to the end of the buffer.
    //  2. While the buffer holds a line break, cut off one complete line.
    //  3. Skip lines that do not start with "data:" (blank keep-alives).
    //  4. Read the rest of the line as JSON. An "error" object fails the
    //     reply. Otherwise every text part of the first candidate is passed
    //     on to the chat.
    //  5. Bytes after the last line break are half a line. They stay in the
    //     buffer until the next readyRead brings the rest.
    auto streamParseBuffer = std::make_shared<QByteArray>();

    QObject::connect(networkReply, &QNetworkReply::readyRead, brainReply,
                     [networkReply, brainReply, streamParseBuffer]() {
        streamParseBuffer->append(networkReply->readAll());

        int lineBreakIndex = streamParseBuffer->indexOf('\n');
        while (lineBreakIndex >= 0) {
            const QByteArray completeLine =
                streamParseBuffer->left(lineBreakIndex).trimmed();
            streamParseBuffer->remove(0, lineBreakIndex + 1);
            lineBreakIndex = streamParseBuffer->indexOf('\n');

            if (!completeLine.startsWith(serverSentEventDataPrefix)) {
                continue; // blank keep-alive lines
            }
            const QByteArray payloadJson =
                completeLine.mid(serverSentEventDataPrefix.size()).trimmed();
            if (payloadJson.isEmpty()) {
                continue;
            }

            const QJsonObject payloadObject =
                QJsonDocument::fromJson(payloadJson).object();

            if (payloadObject.contains(QStringLiteral("error"))) {
                brainReply->markFailed(payloadObject
                    .value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString());
                continue;
            }

            // One chunk can carry several parts; every one of them is text
            // the user is waiting to see.
            const QJsonArray partsArray = payloadObject
                .value(QStringLiteral("candidates")).toArray()
                .at(0).toObject()
                .value(QStringLiteral("content")).toObject()
                .value(QStringLiteral("parts")).toArray();

            for (const QJsonValue &partValue : partsArray) {
                const QString textFragment =
                    partValue.toObject().value(QStringLiteral("text")).toString();
                if (!textFragment.isEmpty()) {
                    brainReply->appendStreamedFragment(textFragment);
                }
            }
        }
    });

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply, modelName]() {
        if (brainReply->isFinished()) {
            return; // already failed mid-stream
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            const int httpStatusCode = networkReply
                ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Google answers a bad key with 400, not 401, so Qt's
            // authentication error never fires and the generic branch would
            // show a network message for what is really a key problem.
            if (isKeyProblemStatus(httpStatusCode)) {
                const QJsonObject errorBodyObject =
                    QJsonDocument::fromJson(networkReply->readAll()).object();
                const QString apiErrorMessage = errorBodyObject
                    .value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString();
                brainReply->markFailed(apiErrorMessage.isEmpty()
                    ? QStringLiteral("Google did not accept this key. Double-check it in "
                                     "Settings — or delete it there and add a fresh one "
                                     "from Google AI Studio.")
                    : apiErrorMessage);
                return;
            }
            if (httpStatusCode == httpNotFound) {
                brainReply->markFailed(
                    QStringLiteral("Google doesn't have a model called %1 for this key. "
                                   "That usually means the model was renamed or isn't "
                                   "available on your plan. Try another brain level in "
                                   "Settings.").arg(modelName));
                return;
            }
            brainReply->markFailed(networkReply->errorString());
            return;
        }

        brainReply->markFinished();
    });

    return brainReply;
}

AiBrainReply *GeminiApiProvider::verifyCredentialConnection(
    const AiCredential &credential, QObject *replyParent)
{
    // Listing models is authenticated, free and small — the cheapest honest
    // way to ask "does this key work?".
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(geminiApiBaseUrl + QStringLiteral("/models"))};
    networkRequest.setRawHeader(geminiApiKeyHeaderName, credential.secretKey.toUtf8());

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(brainReply); // dies with the brain reply

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply]() {
        if (networkReply->error() == QNetworkReply::NoError) {
            brainReply->markFinished();
            return;
        }
        const int httpStatusCode = networkReply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isKeyProblemStatus(httpStatusCode)) {
            brainReply->markFailed(
                QStringLiteral("Google would not accept that key."),
                QStringLiteral(
                    "Go to aistudio.google.com/apikey, copy the key again, then "
                    "delete the old one on the gemini tab above and paste the new "
                    "one in. Two things catch people out here: the key has to "
                    "belong to a project with the Generative Language API turned "
                    "on, and a key restricted by IP address or referrer will be "
                    "refused when a desktop app uses it."));
            return;
        }
        brainReply->markFailed(
            networkReply->errorString(),
            QStringLiteral(
                "That looks like the connection rather than the key. Check that "
                "you are online and tick the box again. On a work laptop or a "
                "VPN, generativelanguage.googleapis.com is sometimes blocked "
                "outright — trying it in a browser will tell you in a few "
                "seconds."));
    });

    return brainReply;
}
