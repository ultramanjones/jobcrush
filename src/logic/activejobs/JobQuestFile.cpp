#include "JobQuestFile.h"

#include <QHash>
#include <QLocale>
#include <QRegularExpression>

#include "../pipelines/TypedDateReader.h"

namespace {

// The fields, as the reader knows them.
enum class QuestField {
    CreatedBy, Company, Title, Link, FoundOn, Status, AppliedOn, Pay, Where, Notes,
    NotAField
};

// The name each field is written under. This is the order a new file is
// written in.
struct FieldName {
    QuestField field;
    QString writtenName;
};

const QList<FieldName> &fieldsInWritingOrder()
{
    static const QList<FieldName> everyField = {
        { QuestField::CreatedBy, QStringLiteral("Quest created by") },
        { QuestField::Company,   QStringLiteral("Company") },
        { QuestField::Title,     QStringLiteral("Title") },
        { QuestField::Link,      QStringLiteral("Link") },
        { QuestField::FoundOn,   QStringLiteral("Found on") },
        { QuestField::Status,    QStringLiteral("Status") },
        { QuestField::AppliedOn, QStringLiteral("Applied on") },
        { QuestField::Pay,       QStringLiteral("Pay") },
        { QuestField::Where,     QStringLiteral("Where") },
        { QuestField::Notes,     QStringLiteral("Notes") },
    };
    return everyField;
}

// Every name the reader accepts for each field, lowercase. The first name of
// each field is the one written above; the rest are names an AI is likely to
// use instead.
QuestField fieldNamed(const QString &lineName)
{
    static const QHash<QString, QuestField> acceptedNames = {
        { QStringLiteral("quest created by"), QuestField::CreatedBy },
        { QStringLiteral("created by"),       QuestField::CreatedBy },
        { QStringLiteral("company"),          QuestField::Company },
        { QStringLiteral("company name"),     QuestField::Company },
        { QStringLiteral("employer"),         QuestField::Company },
        { QStringLiteral("title"),            QuestField::Title },
        { QStringLiteral("job title"),        QuestField::Title },
        { QStringLiteral("position"),         QuestField::Title },
        { QStringLiteral("role"),             QuestField::Title },
        { QStringLiteral("link"),             QuestField::Link },
        { QStringLiteral("url"),              QuestField::Link },
        { QStringLiteral("job link"),         QuestField::Link },
        { QStringLiteral("found on"),         QuestField::FoundOn },
        { QStringLiteral("source"),           QuestField::FoundOn },
        { QStringLiteral("status"),           QuestField::Status },
        { QStringLiteral("stage"),            QuestField::Status },
        { QStringLiteral("applied on"),       QuestField::AppliedOn },
        { QStringLiteral("date applied"),     QuestField::AppliedOn },
        { QStringLiteral("applied"),          QuestField::AppliedOn },
        { QStringLiteral("pay"),              QuestField::Pay },
        { QStringLiteral("salary"),           QuestField::Pay },
        { QStringLiteral("where"),            QuestField::Where },
        { QStringLiteral("location"),         QuestField::Where },
        { QStringLiteral("notes"),            QuestField::Notes },
        { QStringLiteral("note"),             QuestField::Notes },
    };
    return acceptedNames.value(lineName.simplified().toLower(), QuestField::NotAField);
}

// "Name: value" at the start of a line. The name is short letters and spaces,
// and a colon followed by // is a web address, so a link such as https://...
// on a line of its own is never read as a name.
//
// The pattern, piece by piece:
//   ^\s*              the start of the line, then any spaces
//   ([A-Za-z]         capture 1, the name: it starts with a letter,
//    [A-Za-z ]{0,30}?)  then up to 30 more letters or spaces. The ? makes
//                       it take as few as it can, so spaces before the
//                       colon fall to the next piece, not into the name.
//   \s*:              any spaces, then the colon
//   (?!//)            the colon must not be followed by //, or it is a link
//   \s?               one space after the colon, if there is one
//   (.*)$             capture 2, the value: the rest of the line
//
// The limit of 31 letters keeps a sentence with a colon in it, such as a
// note that says "Call them back about the following: pay", from being
// read as a line name.
const QRegularExpression &namedLinePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*([A-Za-z][A-Za-z ]{0,30}?)\\s*:(?!//)\\s?(.*)$"));
    return pattern;
}

// Where one field's text is kept inside a JobQuest. nullptr for NotAField.
QString *fieldInQuest(JobQuest &quest, QuestField field)
{
    switch (field) {
    case QuestField::CreatedBy: return &quest.questCreatedBy;
    case QuestField::Company:   return &quest.companyName;
    case QuestField::Title:     return &quest.positionTitle;
    case QuestField::Link:      return &quest.postingLink;
    case QuestField::FoundOn:   return &quest.foundOn;
    case QuestField::Status:    return &quest.statusText;
    case QuestField::AppliedOn: return &quest.appliedOnText;
    case QuestField::Pay:       return &quest.payText;
    case QuestField::Where:     return &quest.whereText;
    case QuestField::Notes:     return &quest.notesText;
    case QuestField::NotAField: return nullptr;
    }
    return nullptr;
}

// The invisible mark some Windows editors put at the very start of a UTF-8
// file. It is not part of the first line.
constexpr char16_t byteOrderMark = 0xFEFF;

// The text of a file as lines, with a leading byte order mark and Windows
// line endings taken off. Windows ends a line with \r\n and old Macs with \r
// alone. Both become \n before the split.
QStringList linesOf(const QString &fileText)
{
    QString cleanText = fileText;
    if (cleanText.startsWith(QChar(byteOrderMark))) {
        cleanText.remove(0, 1);
    }
    cleanText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    cleanText.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return cleanText.split(QLatin1Char('\n'));
}

// The locale dates in the file are read in: month first, so 10/14/2026 is
// October 14 for every user, whatever their own computer is set to.
const QLocale &monthFirstLocale()
{
    static const QLocale locale(QLocale::English, QLocale::UnitedStates);
    return locale;
}

// One "Name: value" line.
QString writtenLine(const QString &writtenName, const QString &value)
{
    return writtenName + QStringLiteral(": ") + value;
}

// Notes may hold several lines. The first goes on the "Notes:" line and the
// rest follow it, each on its own line. Blank lines are dropped, because the
// reader skips them anyway.
QStringList writtenLinesFor(const QString &writtenName, const QString &value)
{
    QStringList valueLines = value.split(QLatin1Char('\n'));
    QStringList writtenLines;
    writtenLines.append(writtenLine(writtenName, valueLines.takeFirst().trimmed()));
    for (const QString &laterLine : valueLines) {
        // A later line that looks like "Name: value" would be read back as a
        // field. Starting it with two spaces does not stop that, so a dash
        // goes in front instead.
        const QString trimmedLine = laterLine.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }
        if (namedLinePattern().match(trimmedLine).hasMatch()) {
            writtenLines.append(QStringLiteral("- ") + trimmedLine);
        } else {
            writtenLines.append(trimmedLine);
        }
    }
    return writtenLines;
}

} // namespace

namespace JobQuestFile {

// Reads the file one line at a time. Each line is one of three kinds:
//   1. A known "Name: value" line. Its value goes into that field, and that
//      field becomes the one being read.
//   2. An unknown "Name: value" line. It is skipped, and no field is being
//      read after it.
//   3. A line with no name. If a field is being read, the line is added to
//      it on a new line. This is how notes run over several lines.
// After every line is read, the fields are checked and each problem gets a
// sentence.
ReadResult readQuestText(const QString &fileText)
{
    ReadResult result;
    QString *fieldBeingRead = nullptr;

    for (const QString &line : linesOf(fileText)) {
        const QRegularExpressionMatch namedLine = namedLinePattern().match(line);
        if (namedLine.hasMatch()) {
            const QuestField field = fieldNamed(namedLine.captured(1));
            if (field != QuestField::NotAField) {
                fieldBeingRead = fieldInQuest(result.quest, field);
                // A field written twice: the later line wins, the way a person
                // would read it.
                *fieldBeingRead = namedLine.captured(2).trimmed();
                continue;
            }
            // A named line Job Crush does not know. It is kept in the file
            // when Job Crush writes back, and it ends the field above it.
            fieldBeingRead = nullptr;
            continue;
        }

        // A line with no name carries on the field above it.
        const QString trimmedLine = line.trimmed();
        if (fieldBeingRead && !trimmedLine.isEmpty()) {
            if (fieldBeingRead->isEmpty()) {
                *fieldBeingRead = trimmedLine;
            } else {
                *fieldBeingRead += QLatin1Char('\n') + trimmedLine;
            }
        }
    }

    const JobQuest &quest = result.quest;
    if (quest.companyName.isEmpty()) {
        result.problemSentences.append(
            QStringLiteral("There is no Company line. Add one, for example "
                           "\"Company: Acme Robotics\"."));
    }
    if (quest.positionTitle.isEmpty()) {
        result.problemSentences.append(
            QStringLiteral("There is no Title line. Add one, for example "
                           "\"Title: Senior Qt Developer\"."));
    }
    PipelineStage stageRead = PipelineStage::Saved;
    if (!quest.statusText.isEmpty() && !readStatus(quest.statusText, stageRead)) {
        result.problemSentences.append(
            QStringLiteral("Status says \"%1\". Job Crush knows Crushed, Applied, "
                           "Interviewing, Offer and Closed. Change it to one of those.")
                .arg(quest.statusText));
    }
    QDate dateRead;
    if (!quest.appliedOnText.isEmpty()
            && !readAppliedOn(quest.appliedOnText, QDate::currentDate(), dateRead)) {
        result.problemSentences.append(
            QStringLiteral("Applied on says \"%1\". Job Crush could not read that as "
                           "a date. Write it like 10/14/2026.")
                .arg(quest.appliedOnText));
    }

    result.hasWhatTheBoardNeeds = !quest.companyName.isEmpty()
                                  && !quest.positionTitle.isEmpty();
    return result;
}

bool readStatus(const QString &statusText, PipelineStage &stageRead)
{
    static const QHash<QString, PipelineStage> acceptedWords = {
        { QStringLiteral("saved"),        PipelineStage::Saved },
        { QStringLiteral("crushed"),      PipelineStage::Saved },
        { QStringLiteral("not applied"),  PipelineStage::Saved },
        { QStringLiteral("applied"),      PipelineStage::Applied },
        { QStringLiteral("sent"),         PipelineStage::Applied },
        { QStringLiteral("interview"),    PipelineStage::Interview },
        { QStringLiteral("interviewing"), PipelineStage::Interview },
        { QStringLiteral("offer"),        PipelineStage::Offer },
        { QStringLiteral("closed"),       PipelineStage::Closed },
        { QStringLiteral("rejected"),     PipelineStage::Closed },
        { QStringLiteral("withdrawn"),    PipelineStage::Closed },
        { QStringLiteral("declined"),     PipelineStage::Closed },
    };
    // Lowercase, single spaces, and any full stops or exclamation marks at
    // the end taken off, so "Applied." and "APPLIED!" both read as applied.
    const QString cleanWord = statusText.simplified().toLower()
        .remove(QRegularExpression(QStringLiteral("[.!]+$")));
    const auto found = acceptedWords.constFind(cleanWord);
    if (found == acceptedWords.constEnd()) {
        return false;
    }
    stageRead = found.value();
    return true;
}

QString statusWordFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:     return QStringLiteral("Crushed");
    case PipelineStage::Applied:   return QStringLiteral("Applied");
    case PipelineStage::Interview: return QStringLiteral("Interviewing");
    case PipelineStage::Offer:     return QStringLiteral("Offer");
    case PipelineStage::Closed:    return QStringLiteral("Closed");
    }
    return QStringLiteral("Crushed");
}

bool readAppliedOn(const QString &appliedOnText, const QDate &today, QDate &dateRead)
{
    return TypedDateReader::readTypedDate(appliedOnText, today, monthFirstLocale(), dateRead);
}

QString appliedOnTextFor(const QDate &appliedDate)
{
    if (!appliedDate.isValid()) {
        return QString();
    }
    return appliedDate.toString(QStringLiteral("MM/dd/yyyy"));
}

QString questTextFor(const JobQuest &quest)
{
    // A copy, because fieldInQuest hands back a pointer it could write
    // through, and the caller's quest must not change.
    JobQuest questToWrite = quest;
    QStringList lines;
    for (const FieldName &fieldName : fieldsInWritingOrder()) {
        const QString *value = fieldInQuest(questToWrite, fieldName.field);
        if (!value || value->trimmed().isEmpty()) {
            continue;
        }
        lines.append(writtenLinesFor(fieldName.writtenName, *value));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString questTextWithBoardChanges(const QString &existingText,
                                  const QString &statusText,
                                  const QString &appliedOnText,
                                  const QString &notesText)
{
    if (existingText.trimmed().isEmpty()) {
        JobQuest quest;
        quest.statusText = statusText;
        quest.appliedOnText = appliedOnText;
        quest.notesText = notesText;
        return questTextFor(quest);
    }

    const bool fileUsesWindowsLineEndings = existingText.contains(QStringLiteral("\r\n"));
    const QStringList existingLines = linesOf(existingText);

    struct BoardField {
        QuestField field;
        QString writtenName;
        QString value;
        bool written = false;
    };
    QList<BoardField> boardFields = {
        { QuestField::Status,    QStringLiteral("Status"),     statusText },
        { QuestField::AppliedOn, QStringLiteral("Applied on"), appliedOnText },
        { QuestField::Notes,     QStringLiteral("Notes"),      notesText },
    };

    // Walks the old file one line at a time and builds the new one:
    //   - A Status, Applied on or Notes line is replaced by the board's
    //     value. The lines with no name that follow it belonged to its old
    //     value, so they are skipped until the next named line or blank
    //     line. A second copy of the same line is dropped.
    //   - Any other named line is kept as it is.
    //   - A line with no name that is not part of an old board value is kept.
    // A board field with an empty value is left out, which removes its line.
    QStringList newLines;
    bool skippingOldValueLines = false;
    for (const QString &line : existingLines) {
        const QRegularExpressionMatch namedLine = namedLinePattern().match(line);
        if (namedLine.hasMatch()) {
            skippingOldValueLines = false;
            const QuestField field = fieldNamed(namedLine.captured(1));
            BoardField *boardField = nullptr;
            for (BoardField &candidate : boardFields) {
                if (candidate.field == field) {
                    boardField = &candidate;
                }
            }
            if (boardField) {
                // The board's value replaces the line and any lines that
                // carried it on. A second copy of the same line is dropped.
                if (!boardField->written && !boardField->value.trimmed().isEmpty()) {
                    newLines.append(writtenLinesFor(boardField->writtenName,
                                                    boardField->value));
                }
                boardField->written = true;
                skippingOldValueLines = true;
                continue;
            }
            newLines.append(line);
            continue;
        }
        if (skippingOldValueLines && !line.trimmed().isEmpty()) {
            continue;
        }
        skippingOldValueLines = false;
        newLines.append(line);
    }

    // Drop blank lines at the end, add what was missing, and end with one
    // line break.
    while (!newLines.isEmpty() && newLines.last().trimmed().isEmpty()) {
        newLines.removeLast();
    }
    for (const BoardField &boardField : boardFields) {
        if (!boardField.written && !boardField.value.trimmed().isEmpty()) {
            newLines.append(writtenLinesFor(boardField.writtenName, boardField.value));
        }
    }

    const QString lineEnding = fileUsesWindowsLineEndings ? QStringLiteral("\r\n")
                                                          : QStringLiteral("\n");
    return newLines.join(lineEnding) + lineEnding;
}

} // namespace JobQuestFile
