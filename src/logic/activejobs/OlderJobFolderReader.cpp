#include "OlderJobFolderReader.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// What splits the company from the title in an older folder's name.
const QString companyTitleSeparator = QStringLiteral(" - ");

// The value of a line like "PAY: $140k", matched without regard to case.
// Empty when there is no such line.
//
// The pattern: the start of the line, any spaces, the line name (escaped, so
// a name with a dot in it is matched as written), any spaces, a colon, any
// spaces, then capture 1, the rest of the line.
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
//
// The pattern: "http", an optional "s", "://", then every character up to the
// first space, angle bracket or quote mark. Those are the characters that end
// a link written in plain text or in HTML.
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
    // A link at the end of a sentence carries the sentence's full stop. A
    // link in brackets carries the closing bracket. Both are taken off.
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
    // The pattern: digits at the start, any spaces, a dash, any spaces.
    static const QRegularExpression leadingNumber(QStringLiteral("^\\d+\\s*-\\s*"));
    QString folderName = jobFolder.dirName();
    folderName.remove(leadingNumber);

    // Everything before the first " - " is the company. Everything after it
    // is the title. A separator at the very start means there is no company.
    const int splitAt = folderName.indexOf(companyTitleSeparator);
    if (splitAt <= 0) {
        problemSentence = QStringLiteral(
            "The folder name does not say the company and the title. Rename it like "
            "\"Acme Robotics - Senior Qt Developer\" and import it again.");
        return false;
    }

    questRead = JobQuest{};
    questRead.companyName = folderName.left(splitAt).trimmed();
    questRead.positionTitle =
        folderName.mid(splitAt + companyTitleSeparator.size()).trimmed();
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
    } else {
        // The folder name still gives the company and the title, so the job
        // is brought in without the link, pay and place.
        qWarning().noquote() << QStringLiteral(
            "OlderJobFolderReader::readOlderJobFolder: could not open \"%1\" — %2")
                .arg(howToApplyFile.fileName(), howToApplyFile.errorString());
    }

    const QDateTime folderChangedTimestamp = QFileInfo(jobFolder.absolutePath()).lastModified();
    questRead.appliedOnText = JobQuestFile::appliedOnTextFor(folderChangedTimestamp.date());
    return true;
}

} // namespace OlderJobFolderReader
