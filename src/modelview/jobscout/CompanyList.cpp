#include "CompanyList.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace {

// "Collins Aerospace (RTX), Inc." -> "collinsaerospace"
QString plainNameOf(const QString &companyName)
{
    QString plainName = companyName.toLower();
    plainName.remove(QRegularExpression(QStringLiteral("\\(.*?\\)")));
    static const QRegularExpression legalWords(
        QStringLiteral("\\b(inc|incorporated|llc|ltd|corp|corporation|co|company|plc|group)\\b"));
    plainName.remove(legalWords);
    plainName.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return plainName;
}

} // namespace

bool CompanyList::readFromFile(const QString &filePath, CompanyList &listRead,
                               QString &problemText)
{
    QFile listFile(filePath);
    if (!listFile.open(QIODevice::ReadOnly)) {
        problemText = QStringLiteral("Job Crush could not open the company list at %1.")
                          .arg(filePath);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument listDocument = QJsonDocument::fromJson(listFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !listDocument.isObject()) {
        problemText = QStringLiteral("The company list at %1 is not a list Job Crush can "
                                     "read: %2.").arg(filePath, parseError.errorString());
        return false;
    }

    const QJsonObject listObject = listDocument.object();
    listRead = CompanyList{};
    listRead.listName = listObject.value(QStringLiteral("listName")).toString();
    listRead.about = listObject.value(QStringLiteral("about")).toString();
    for (const QJsonValue &companyValue : listObject.value(QStringLiteral("companies")).toArray()) {
        const QJsonObject companyObject = companyValue.toObject();
        CompanyListEntry entry;
        entry.companyName = companyObject.value(QStringLiteral("name")).toString().trimmed();
        entry.sector = companyObject.value(QStringLiteral("sector")).toString().trimmed();
        entry.hiringSystem =
            companyObject.value(QStringLiteral("hiringSystem")).toString().trimmed();
        entry.followLink = companyObject.value(QStringLiteral("followLink")).toString().trimmed();
        entry.lastChecked =
            companyObject.value(QStringLiteral("lastChecked")).toString().trimmed();
        entry.note = companyObject.value(QStringLiteral("note")).toString().trimmed();
        if (!entry.companyName.isEmpty()) {
            listRead.companies.append(entry);
        }
    }
    return true;
}

bool CompanyList::findCompany(const QString &companyName, CompanyListEntry &entryFound) const
{
    const QString wantedName = plainNameOf(companyName);
    if (wantedName.isEmpty()) {
        return false;
    }
    for (const CompanyListEntry &entry : companies) {
        if (plainNameOf(entry.companyName) == wantedName) {
            entryFound = entry;
            return true;
        }
    }
    return false;
}
