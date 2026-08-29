#include "DocumentConverter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace {

// A line of nothing but rule characters. In the source document it drew a
// line across the page; as markdown it would become a page break, which is
// how a one-page resume turns into three. Dropped instead.
bool lineIsOnlyARule(const QString &line)
{
    if (line.length() < 2) {
        return false;
    }
    static const QRegularExpression ruleCharacters(
        QStringLiteral("^[-_=~*.\u00B7\u2022\u2013\u2014[:space:]]+$"));
    return ruleCharacters.match(line).hasMatch();
}

// The bullet characters that actually turn up in resumes, plus the plain
// hyphen and asterisk somebody typed by hand.
bool lineStartsWithABullet(const QString &line)
{
    if (line.length() < 2) {
        return false;
    }
    static const QString bulletCharacters =
        QStringLiteral("-*•●▪·‣⁃–—");
    return bulletCharacters.contains(line.at(0)) && line.at(1).isSpace();
}

// A short line in capitals is a section heading: EXPERIENCE, EDUCATION,
// SKILLS. Long ones are not - a whole sentence somebody shouted is still a
// sentence, and a company name in capitals on its own line reads fine as a
// heading anyway.
bool lineLooksLikeAHeading(const QString &line)
{
    if (line.length() > 40) {
        return false;
    }
    bool sawALetter = false;
    for (const QChar &character : line) {
        if (character.isLetter()) {
            sawALetter = true;
            if (character.isLower()) {
                return false;
            }
        }
    }
    return sawALetter;
}

// Markdown reads some characters as instructions. A resume line beginning
// "#1 salesperson" must not become a heading, and "> 10 years" must not
// become a quote.
QString withLeadingMarkdownDefused(const QString &line)
{
    if (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('>'))) {
        return QStringLiteral("\\") + line;
    }
    return line;
}

} // namespace

QString DocumentConverter::markdownForPlainText(const QString &plainText)
{
    QStringList markdownLines;

    const QStringList sourceLines = plainText.split(QLatin1Char('\n'));
    for (const QString &sourceLine : sourceLines) {
        const QString line = sourceLine.trimmed();

        if (line.isEmpty() || lineIsOnlyARule(line)) {
            continue;
        }

        if (lineStartsWithABullet(line)) {
            markdownLines.append(QStringLiteral("- ") + line.mid(1).trimmed());
            continue;
        }

        if (lineLooksLikeAHeading(line)) {
            markdownLines.append(QStringLiteral("## ") + line);
            continue;
        }

        markdownLines.append(withLeadingMarkdownDefused(line));
    }

    // Every line stands on its own.
    //
    // The alternative is to join neighbouring lines back into paragraphs, and
    // that guesses wrong on the documents that matter most: a resume is short
    // lines by design, and gluing "Acme Robotics" onto "Senior Engineer,
    // 2019-2024" wrecks it. Keeping the lines apart is honest about what was
    // read and never invents a sentence the person did not write.
    return markdownLines.join(QStringLiteral("\n\n"));
}

DocumentConverter::ConversionOutcome DocumentConverter::convertToFile(
    const QString &documentText,
    const QString &suggestedBaseName,
    const QString &format,
    const QString &destinationFolderPath) const
{
    ConversionOutcome outcome;

    if (documentText.trimmed().isEmpty()) {
        outcome.reasonText =
            QStringLiteral("There are no words in this document to convert.");
        outcome.whatToDoNextText =
            QStringLiteral("If it is a scan, it is a picture of a page rather than "
                           "words, and there is nothing to put in a Word file yet. "
                           "Drop in the original digital copy if you have one.");
        return outcome;
    }

    if (!ExportFormat::isKnownConversionFormat(format)) {
        outcome.reasonText =
            QStringLiteral("Job Crush doesn't write .%1 files.").arg(format);
        outcome.whatToDoNextText =
            QStringLiteral("Pick Word, PDF or plain text instead.");
        return outcome;
    }

    QDir destinationFolder(destinationFolderPath);
    if (!destinationFolder.exists() && !destinationFolder.mkpath(QStringLiteral("."))) {
        outcome.reasonText = QStringLiteral("Job Crush couldn't create the folder %1.")
                                 .arg(destinationFolderPath);
        outcome.whatToDoNextText =
            QStringLiteral("Pick a different folder in Settings, or check whether that "
                           "one is somewhere you're allowed to write.");
        return outcome;
    }

    const QString extension = ExportFormat::fileExtensionFor(format);

    // Never overwrite. A second copy is somebody making a new version, not
    // throwing the old one away, and the old one may be the one already
    // attached to an application.
    QString fileName = QStringLiteral("%1.%2").arg(suggestedBaseName, extension);
    int duplicateNumber = 2;
    while (destinationFolder.exists(fileName)) {
        fileName = QStringLiteral("%1 (%2).%3")
                       .arg(suggestedBaseName)
                       .arg(duplicateNumber)
                       .arg(extension);
        ++duplicateNumber;
    }
    const QString filePath = destinationFolder.filePath(fileName);

    QString reasonText;
    bool wasWritten = false;

    if (format == ExportFormat::PlainTextDocument) {
        // The words as they were read, not the markdown. Nobody wants "##" in
        // a file they are about to paste into an application form.
        QFile plainTextFile(filePath);
        if (plainTextFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream textStream(&plainTextFile);
            textStream << documentText;
            plainTextFile.close();
            wasWritten = true;
        } else {
            reasonText = QStringLiteral("Job Crush couldn't write %1 - %2")
                             .arg(fileName, plainTextFile.errorString());
        }
    } else {
        const QString markdownText = markdownForPlainText(documentText);
        wasWritten = (format == ExportFormat::PortableDocument)
            ? documentWriter.writePortableDocument(markdownText, filePath, reasonText)
            : documentWriter.writeWordDocument(markdownText, filePath, reasonText);
    }

    if (!wasWritten) {
        outcome.reasonText = reasonText;
        outcome.whatToDoNextText =
            QStringLiteral("If a file of that name is open in another program, close it "
                           "and try again.");
        return outcome;
    }

    outcome.succeeded = true;
    outcome.writtenFilePath = filePath;
    return outcome;
}
