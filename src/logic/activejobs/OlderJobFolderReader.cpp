#include "OlderJobFolderReader.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// The value of a line like "PAY: $140k", matched without regard to case.
// Empty when there is no such line.
QString valueOfLine(const QStringList &lines, const QString &lineName)
{
    const QRegularExpression linePattern(
        QStringLiteral("^\\s*%1\\s*:\\s*(.*)$").arg(QRegularExpression::escape(lineName)),
        QRegularExpression::CaseInsensitiveOption);
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = linePattern.match(line);
        if (match.hasMatch()) {
            return match.captured(1).trimmed();
        }
    }
    return QString();
}

// The first web address in some text.
QString firstLinkIn(const QString &text)
{
    static const QRegularExpression linkPattern(
        QStringLiteral("https?://[^\\s<>\"']+"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = linkPattern.match(text);
    if (!match.hasMatch()) {
        return QString();
    }
    QString link = match.captured(0);
    // A link at the end of a sentence carries the sentence's full stop.
    while (link.endsWith(QLatin1Char('.')) || link.endsWith(QLatin1Char(','))
           || link.endsWith(QLatin1Char(')'))) {
        link.chop(1);
    }
    return link;
}

} // namespace

namespace OlderJobFolderReader {

bool isOlderJobFolder(const QDir &jobFolder)
{
    return jobFolder.exists(HowToApplyFileName)
           && !jobFolder.exists(JobQuestFile::FileName);
}

bool readOlderJobFolder(const QDir &jobFolder, JobQuest &questRead,
                        QString &problemSentence)
{
    // Some older folders start with a number, like "04 - Acme - Qt Developer".
    // The number is only for sorting, so drop it before reading the company.
    static const QRegularExpression leadingNumber(QStringLiteral("^\\d+\\s*-\\s*"));
    QString folderName = jobFolder.dirName();
    folderName.remove(leadingNumber);
    const int splitAt = folderName.indexOf(QStringLiteral(" - "));
    if (splitAt <= 0) {
        problemSentence = QStringLiteral(
            "The folder name does not say the company and the title. Rename it like "
            "\"Acme Robotics - Senior Qt Developer\" and import it again.");
        return false;
    }

    questRead = JobQuest{};
    questRead.companyName = folderName.left(splitAt).trimmed();
    questRead.positionTitle = folderName.mid(splitAt + 3).trimmed();
    questRead.questCreatedBy = QStringLiteral("Imported");
    questRead.statusText = JobQuestFile::statusWordFor(PipelineStage::Applied);

    QFile howToApplyFile(jobFolder.filePath(HowToApplyFileName));
    if (howToApplyFile.open(QIODevice::ReadOnly)) {
        QString fileText = QString::fromUtf8(howToApplyFile.readAll());
        fileText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        const QStringList lines = fileText.split(QLatin1Char('\n'));

        const QString howLine = valueOfLine(lines, QStringLiteral("HOW"));
        questRead.postingLink = firstLinkIn(howLine);
        if (questRead.postingLink.isEmpty()) {
            // The link was not on the HOW: line. Take the first one anywhere.
            questRead.postingLink = firstLinkIn(fileText);
        }
        questRead.payText = valueOfLine(lines, QStringLiteral("PAY"));
        questRead.whereText = valueOfLine(lines, QStringLiteral("WHERE"));
    }

    const QDateTime folderChanged = QFileInfo(jobFolder.absolutePath()).lastModified();
    questRead.appliedOnText = JobQuestFile::appliedOnTextFor(folderChanged.date());
    return true;
}

} // namespace OlderJobFolderReader
