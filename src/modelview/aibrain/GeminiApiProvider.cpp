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

// The model Brain Chat speaks to. One named place to change it — and the
// per-provider model picker replaces this constant, exactly as it will
// replace the Anthropic and OpenRouter ones.
const QString geminiDefaultModelName = QStringLiteral("gemini-2.5-flash");

constexpr int geminiMaxResponseTokens = 2048;

// Google reads the key from this header. Their own samples put it in the URL
// instead, which is how API keys end up in server logs and screenshots.
const QByteArray geminiApiKeyHeaderName = QByteArrayLiteral("x-goog-api-key");

} // namespace

GeminiApiProvider::GeminiApiProvider() = default;

QString GeminiApiProvider::providerDisplayName() const
{
    return QStringLiteral("Gemini");
}

QByteArray GeminiApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation) const
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
    generationConfigObject.insert(QStringLiteral("maxOutputTokens"), geminiMaxResponseTokens);
    requestBodyObject.insert(QStringLiteral("generationConfig"), generationConfigObject);

    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *GeminiApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    // alt=sse is what turns this into a stream. Without it Google sends one
    // enormous JSON array at the end, and the chat would sit silent until the
    // whole answer existed — which is the frozen-looking wait the no-spinner
    // law exists to prevent.
    const QUrl requestUrl(QStringLiteral("%1/models/%2:streamGenerateContent?alt=sse")
                              .arg(geminiApiBaseUrl, geminiDefaultModelName));

    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(geminiApiKeyHeaderName, credential.secretKey.toUtf8());

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation));
    networkReply->setParent(brainReply); // dies with the brain reply

    // --- Google's server-sent events -------------------------------------
    //
    //     data: {"candidates":[{"content":{"parts":[{"text":"Hel"}]}}]}
    //
    // Same rolling-buffer technique as the other two providers: cut complete
    // lines, forward every text part, keep partial lines for later.
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

            if (!completeLine.startsWith("data:")) {
                continue; // blank keep-alive lines
            }
            const QByteArray payloadJson = completeLine.mid(5).trimmed();
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
                     [networkReply, brainReply]() {
        if (brainReply->isFinished()) {
            return; // already failed mid-stream
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            const int httpStatusCode = networkReply
                ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Google answers a bad key with 400, not 401, so Qt's
            // authentication error never fires and the generic branch would
            // show a network message for what is really a key problem.
            if (httpStatusCode == 400 || httpStatusCode == 401 || httpStatusCode == 403) {
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
            if (httpStatusCode == 404) {
                brainReply->markFailed(
                    QStringLiteral("Google doesn't have a model called %1 for this key. "
                                   "That usually means the model was renamed or isn't "
                                   "available on your plan.").arg(geminiDefaultModelName));
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
    networkReply->setParent(brainReply);

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply]() {
        if (networkReply->error() == QNetworkReply::NoError) {
            brainReply->markFinished();
            return;
        }
        const int httpStatusCode = networkReply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatusCode == 400 || httpStatusCode == 401 || httpStatusCode == 403) {
            brainReply->markFailed(QStringLiteral(
                "Google did not accept this key. Delete it in Settings and add a fresh "
                "one from Google AI Studio."));
            return;
        }
        brainReply->markFailed(networkReply->errorString());
    });

    return brainReply;
}
