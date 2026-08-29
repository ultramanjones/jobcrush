#include <QGuiApplication>
#include <QIcon>
#include <QUrl>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#include "model/JobCrushDatabase.h"
#include "model/JobPostingRepository.h"
#include "model/JobApplicationRepository.h"
#include "model/ProfessionalDocumentRepository.h"
#include "model/CareerHistoryRepository.h"
#include "model/StagedDocumentRepository.h"

#include "modelview/AppPreferences.h"
#include "modelview/aibrain/AiBrain.h"
#include "modelview/aibrain/AiBrainSoul.h"
#include "modelview/pipelines/JobPipelines.h"
#include "modelview/aibrain/AiCredentialRoster.h"
#include "modelview/brainchat/BrainChatSession.h"
#include "modelview/jobscout/JobScout.h"
#include "modelview/jobscout/JobSearchProfile.h"
#include "modelview/jobscout/FollowedEmployerRoster.h"
#include "modelview/jobscout/JobSourceRoster.h"
#include "modelview/prodocs/ProDocsIntake.h"
#include "modelview/stats/JobSearchStatistics.h"
#include "modelview/tasks/StagingWorkbench.h"

#include "viewmodel/AiCredentialRosterViewModel.h"
#include "viewmodel/AppPreferencesViewModel.h"
#include "viewmodel/BrainChatConversationViewModel.h"
#include "viewmodel/SelectedBrainConnectionViewModel.h"
#include "viewmodel/DiscoveredJobListViewModel.h"
#include "viewmodel/JobPipelineBoardViewModel.h"
#include "viewmodel/JobSearchProfileViewModel.h"
#include "viewmodel/FollowedEmployerListViewModel.h"
#include "viewmodel/JobSourceRosterViewModel.h"
#include "viewmodel/ProfessionalDocumentListViewModel.h"
#include "viewmodel/WorkExperienceListViewModel.h"
#include "viewmodel/EducationListViewModel.h"
#include "viewmodel/StagedJobListViewModel.h"
#include "viewmodel/StagingPacketViewModel.h"
#include "viewmodel/JobSearchStatsViewModel.h"

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

    // The taskbar / alt-tab / title-bar icon while the app is running. The
    // executable's own icon is a separate thing set in assets/jobcrush.rc.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/jobcrush-icon.png")));

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
    ProfessionalDocumentRepository professionalDocumentRepository(jobCrushDatabase);
    CareerHistoryRepository careerHistoryRepository(jobCrushDatabase);
    StagedDocumentRepository stagedDocumentRepository(jobCrushDatabase);

    // (The Job Pipelines board arrives in Phase 4; its repository stays wired
    //  so the startup path is proven end to end.)
    Q_UNUSED(jobApplicationRepository);

    // --- ModelView layer: preferences, AIBrain, Brain Chat -----------------
    AppPreferences appPreferences;
    appPreferences.loadFromSettings();

    AiCredentialRoster aiCredentialRoster;
    aiCredentialRoster.loadFromSettings();

    AiBrainSoul aiBrainSoul(
        QDir(applicationDataFolderPath).filePath(QStringLiteral("soul")));
    aiBrainSoul.loadCreatingDefaultsIfMissing();

    ProDocsIntake proDocsIntake(professionalDocumentRepository, careerHistoryRepository,
                                applicationDataFolderPath);

    // A database that got itself into a mess before the guard existed heals
    // here rather than leaving the user to delete rows by hand. Re-reading
    // documents used to duplicate every entry the user had confirmed, so
    // there are databases in the wild carrying twins of everything.
    careerHistoryRepository.removeDuplicateEntries();

    // Anything dropped before Job Crush knew how to read jobs and schooling
    // out of documents gets read now, once, so the Experience tab is
    // populated the first time the user opens it rather than sitting empty
    // behind a button nobody knew to press.
    // The resume reader improves between builds. When the running one is
    // newer than the one that last read this database, its predecessor's work
    // is thrown out and every document is read again — so a mistake fixed in
    // the code actually reaches the person who was living with it, rather
    // than waiting for them to notice a button.
    proDocsIntake.rereadEverythingIfTheReaderImproved();

    proDocsIntake.readAnyDocumentsNotReadYet();

    JobSearchProfile jobSearchProfile;
    jobSearchProfile.loadFromSettings();

    JobSourceRoster jobSourceRoster;
    jobSourceRoster.loadFromSettings();

    FollowedEmployerRoster followedEmployerRoster;
    followedEmployerRoster.loadFromSettings();

    JobScout jobScout(jobPostingRepository, jobSourceRoster, followedEmployerRoster,
                      jobSearchProfile,
                      applicationDataFolderPath);

    // The board. Loaded now so every screen that asks "is this job already on
    // my board?" gets a straight answer from the first frame.
    JobPipelines jobPipelines(jobApplicationRepository, jobPostingRepository);
    jobPipelines.loadFromDatabase();

    AiBrain aiBrain(aiCredentialRoster, aiBrainSoul);
    aiBrain.loadFromSettings(); // which brain the user chose last time

    BrainChatSession brainChatSession(aiBrain);

    // The packet bench. Built after AIBrain because it asks the brain to do
    // the work, and after the board because a packet belongs to a campaign.
    StagingWorkbench stagingWorkbench(aiBrain, jobPipelines, stagedDocumentRepository,
                                      jobApplicationRepository, careerHistoryRepository,
                                      professionalDocumentRepository);

    // Crushing a job starts its packet. The checklist costs nothing and needs
    // no brain, so every crushed job has something in it from the first
    // moment — and the two cheap questions (what is this, am I in with a
    // chance) are asked straight away when a brain is connected.
    QObject::connect(&jobPipelines, &JobPipelines::jobWasCrushed,
                     &stagingWorkbench, &StagingWorkbench::startPacketFor);

    JobSearchStatistics jobSearchStatistics(jobPipelines, stagedDocumentRepository);

    // --- ViewModel layer ----------------------------------------------------
    BrainChatConversationViewModel brainChatConversationViewModel(
        brainChatSession, aiBrain);
    AiCredentialRosterViewModel aiCredentialRosterViewModel(aiCredentialRoster);
    AppPreferencesViewModel appPreferencesViewModel(appPreferences, aiBrainSoul);
    SelectedBrainConnectionViewModel selectedBrainConnectionViewModel(aiBrain);
    DiscoveredJobListViewModel discoveredJobListViewModel(jobScout, jobPipelines);
    JobPipelineBoardViewModel jobPipelineBoardViewModel(jobPipelines);
    JobSourceRosterViewModel jobSourceRosterViewModel(jobSourceRoster);
    FollowedEmployerListViewModel followedEmployerListViewModel(followedEmployerRoster);
    JobSearchProfileViewModel jobSearchProfileViewModel(jobSearchProfile);
    ProfessionalDocumentListViewModel professionalDocumentListViewModel(
        professionalDocumentRepository, proDocsIntake, appPreferences,
        applicationDataFolderPath);
    WorkExperienceListViewModel workExperienceListViewModel(
        careerHistoryRepository, proDocsIntake);
    EducationListViewModel educationListViewModel(careerHistoryRepository, proDocsIntake);
    StagedJobListViewModel stagedJobListViewModel(jobPipelines, stagedDocumentRepository,
                                                  stagingWorkbench);
    StagingPacketViewModel stagingPacketViewModel(stagingWorkbench, stagedDocumentRepository,
                                                  jobPipelines, appPreferences,
                                                  applicationDataFolderPath);
    JobSearchStatsViewModel jobSearchStatsViewModel(jobSearchStatistics, stagingWorkbench);

    // --- View layer: the QML engine ----------------------------------------
    QQmlApplicationEngine qmlEngine;
    QObject::connect(
        &qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    qmlEngine.setInitialProperties({
        { QStringLiteral("brainChatConversationViewModel"),
          QVariant::fromValue(&brainChatConversationViewModel) },
        { QStringLiteral("jobPipelineBoardViewModel"),
          QVariant::fromValue(&jobPipelineBoardViewModel) },
        { QStringLiteral("aiCredentialRosterViewModel"),
          QVariant::fromValue(&aiCredentialRosterViewModel) },
        { QStringLiteral("appPreferencesViewModel"),
          QVariant::fromValue(&appPreferencesViewModel) },
        { QStringLiteral("selectedBrainConnectionViewModel"),
          QVariant::fromValue(&selectedBrainConnectionViewModel) },
        { QStringLiteral("discoveredJobListViewModel"),
          QVariant::fromValue(&discoveredJobListViewModel) },
        { QStringLiteral("jobSourceRosterViewModel"),
          QVariant::fromValue(&jobSourceRosterViewModel) },
        { QStringLiteral("followedEmployerListViewModel"),
          QVariant::fromValue(&followedEmployerListViewModel) },
        { QStringLiteral("jobSearchProfileViewModel"),
          QVariant::fromValue(&jobSearchProfileViewModel) },
        { QStringLiteral("professionalDocumentListViewModel"),
          QVariant::fromValue(&professionalDocumentListViewModel) },
        { QStringLiteral("workExperienceListViewModel"),
          QVariant::fromValue(&workExperienceListViewModel) },
        { QStringLiteral("educationListViewModel"),
          QVariant::fromValue(&educationListViewModel) },
        { QStringLiteral("stagedJobListViewModel"),
          QVariant::fromValue(&stagedJobListViewModel) },
        { QStringLiteral("stagingPacketViewModel"),
          QVariant::fromValue(&stagingPacketViewModel) },
        { QStringLiteral("jobSearchStatsViewModel"),
          QVariant::fromValue(&jobSearchStatsViewModel) },
    });

    qmlEngine.loadFromModule("JobCrush", "Main");

    return application.exec();
}
