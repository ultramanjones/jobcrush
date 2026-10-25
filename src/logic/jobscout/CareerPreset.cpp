#include "CareerPreset.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

// The strings in a JSON array, trimmed, with empty ones dropped. Anything
// that is not an array gives an empty list.
QStringList textListFrom(const QJsonValue &value)
{
    QStringList texts;
    for (const QJsonValue &item : value.toArray()) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            texts.append(text);
        }
    }
    return texts;
}

} // namespace

bool CareerPreset::readFromFile(const QString &filePath, CareerPreset &presetRead,
                                QString &problemText)
{
    QFile presetFile(filePath);
    if (!presetFile.open(QIODevice::ReadOnly)) {
        problemText = QStringLiteral("Job Crush could not open the preset at %1.").arg(filePath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument presetDocument = QJsonDocument::fromJson(presetFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !presetDocument.isObject()) {
        problemText = QStringLiteral("The preset at %1 is not one Job Crush can read: %2.")
                          .arg(filePath, parseError.errorString());
        return false;
    }
    const QJsonObject presetObject = presetDocument.object();
    presetRead = CareerPreset{};
    presetRead.presetName = presetObject.value(QStringLiteral("presetName")).toString();
    presetRead.about = presetObject.value(QStringLiteral("about")).toString();
    presetRead.jobTitles = textListFrom(presetObject.value(QStringLiteral("jobTitles")));
    presetRead.strongSkills = textListFrom(presetObject.value(QStringLiteral("strongSkills")));
    presetRead.goodSkills = textListFrom(presetObject.value(QStringLiteral("goodSkills")));
    return true;
}
