#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#include "model/JobCrushDatabase.h"
#include "model/JobPostingRepository.h"
#include "model/JobApplicationRepository.h"

#include "modelview/AppPreferences.h"
#include "modelview/aibrain/AiBrain.h"
#include "modelview/aibrain/AiBrainSoul.h"
#include "modelview/aibrain/AiCredentialRoster.h"
#include "modelview/brainchat/BrainChatSession.h"

#include "viewmodel/AiCredentialRosterViewModel.h"
#include "viewmodel/AppPreferencesViewModel.h"
#include "viewmodel/BrainChatConversationViewModel.h"

// main — the composition root.
//
// This is the ONE place where the layers get wired together, by hand,
// in stack order from the metal up:
//
//     database -> repositories -> modelviews -> viewmodels -> QML engine
//
// No dependency framework, no container — just constructors.
int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Ultra"));
    QGuiApplication::setApplicationName(QStringLiteral("Job Crush"));

    // --- Model layer: the database, close to the metal --------------------
    const QString applicationDataFolderPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString databaseFilePath =
        QDir(applicationDataFolderPath).filePath(QStringLiteral("jobcrush.sqlite"));

    JobCrushDatabase jobCrushDatabase;
    if (!jobCrushDatabase.openAtFilePath(databaseFilePath)) {
        qCritical() << "Job Crush could not open its database:"
                    << jobCrushDatabase.lastErrorText();
        return -1;
    }
    qInfo() << "Job Crush database ready at" << databaseFilePath;

    // --- Model layer: repositories ----------------------------------------
    JobPostingRepository jobPostingRepository(jobCrushDatabase);
    JobApplicationRepository jobApplicationRepository(jobCrushDatabase);

    // (The board arrives in Phase 4; the repositories stay wired so the
    //  startup path is proven end to end.)
    Q_UNUSED(jobPostingRepository);
    Q_UNUSED(jobApplicationRepository);

    // --- ModelView layer: preferences, AIBrain, Brain Chat -----------------
    AppPreferences appPreferences;
    appPreferences.loadFromSettings();

    AiCredentialRoster aiCredentialRoster;
    aiCredentialRoster.loadFromSettings();

    AiBrainSoul aiBrainSoul(
        QDir(applicationDataFolderPath).filePath(QStringLiteral("soul")));
    aiBrainSoul.loadCreatingDefaultsIfMissing();

    AiBrain aiBrain(aiCredentialRoster, aiBrainSoul);
    BrainChatSession brainChatSession(aiBrain);

    // --- ViewModel layer ----------------------------------------------------
    BrainChatConversationViewModel brainChatConversationViewModel(
        brainChatSession, aiBrain);
    AiCredentialRosterViewModel aiCredentialRosterViewModel(aiCredentialRoster);
    AppPreferencesViewModel appPreferencesViewModel(appPreferences, aiBrainSoul);

    // --- View layer: the QML engine ----------------------------------------
    QQmlApplicationEngine qmlEngine;
    QObject::connect(
        &qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    qmlEngine.setInitialProperties({
        { QStringLiteral("brainChatConversationViewModel"),
          QVariant::fromValue(&brainChatConversationViewModel) },
        { QStringLiteral("aiCredentialRosterViewModel"),
          QVariant::fromValue(&aiCredentialRosterViewModel) },
        { QStringLiteral("appPreferencesViewModel"),
          QVariant::fromValue(&appPreferencesViewModel) },
    });

    qmlEngine.loadFromModule("JobCrush", "Main");

    return application.exec();
}
