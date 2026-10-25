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
#include "model/ActiveJobFolderRepository.h"

#include "logic/AppPreferences.h"
#include "logic/activejobs/ActiveJobsFolder.h"
#include "logic/aiapps/AiAppConnection.h"
#include "logic/aiapps/AiAppRequests.h"
#include "logic/aibrain/AiBrain.h"
#include "logic/aibrain/AiBrainSoul.h"
#include "model/PostingSourceRepository.h"
#include "logic/pipelines/JobPipelines.h"
#include "logic/pipelines/PostingStillListedChecker.h"
#include "logic/aibrain/AiCredentialRoster.h"
#include "logic/brainchat/BrainChatSession.h"
#include "logic/jobscout/JobScout.h"
#include "logic/jobscout/JobSearchProfile.h"
#include "logic/jobscout/FollowedEmployerRoster.h"
#include "logic/jobscout/JobSourceRoster.h"
#include "logic/prodocs/MoonlightDocumentCleanup.h"
#include "logic/prodocs/ProDocsIntake.h"
#include "logic/stats/JobSearchStatistics.h"
#include "logic/tasks/StagingWorkbench.h"

#include "viewmodel/ActiveJobsFolderViewModel.h"
#include "viewmodel/AiAppConnectionViewModel.h"
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
//     database -> repositories -> logic classes -> viewmodels -> QML engine
//
// No dependency framework and no container. Only constructors.
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
        qCritical().noquote() << QStringLiteral(
            "main: could not open the database at %1 — %2")
                .arg(databaseFilePath, jobCrushDatabase.lastErrorText());
        return -1;
    }
    qInfo() << "Job Crush database ready at" << databaseFilePath;

    // --- Model layer: repositories ----------------------------------------
    JobPostingRepository jobPostingRepository(jobCrushDatabase);
    PostingSourceRepository postingSourceRepository(jobCrushDatabase);
    JobApplicationRepository jobApplicationRepository(jobCrushDatabase);
    ProfessionalDocumentRepository professionalDocumentRepository(jobCrushDatabase);
    CareerHistoryRepository careerHistoryRepository(jobCrushDatabase);
    StagedDocumentRepository stagedDocumentRepository(jobCrushDatabase);
    ActiveJobFolderRepository activeJobFolderRepository(jobCrushDatabase);

    // --- Logic layer: preferences, AIBrain, Brain Chat -----------------
    AppPreferences appPreferences;
    appPreferences.loadFromSettings();

    AiCredentialRoster aiCredentialRoster;
    aiCredentialRoster.loadFromSettings();

    AiBrainSoul aiBrainSoul(
        QDir(applicationDataFolderPath).filePath(QStringLiteral("soul")));
    aiBrainSoul.loadCreatingDefaultsIfMissing();

    ProDocsIntake proDocsIntake(professionalDocumentRepository, careerHistoryRepository,
                                applicationDataFolderPath);

    // Re-reading documents used to duplicate every entry the user had
    // confirmed, so some databases hold two of everything. They are cleaned
    // up here, so the user does not have to delete rows by hand.
    if (careerHistoryRepository.removeDuplicateEntries() < 0) {
        qWarning().noquote() << QStringLiteral(
            "main: could not remove duplicate career history entries from %1 — %2")
                .arg(databaseFilePath, careerHistoryRepository.lastErrorText());
    }

    // The resume reader improves between builds. When the running one is
    // newer than the one that last read this database, the old reader's work
    // is thrown out and every document is read again. That way a mistake
    // fixed in the code reaches the user without them pressing a button.
    proDocsIntake.rereadEverythingIfTheReaderImproved();

    // Anything dropped before Job Crush knew how to read jobs and schooling
    // out of documents gets read now, once. The Experience tab is then full
    // the first time the user opens it, not empty behind a button nobody
    // knew to press.
    proDocsIntake.readAnyDocumentsNotReadYet();

    JobSearchProfile jobSearchProfile;
    jobSearchProfile.loadFromSettings();

    JobSourceRoster jobSourceRoster;
    jobSourceRoster.loadFromSettings();

    FollowedEmployerRoster followedEmployerRoster;
    followedEmployerRoster.loadFromSettings();

    AiBrain aiBrain(aiCredentialRoster, aiBrainSoul);
    aiBrain.loadFromSettings(); // which brain the user chose last time

    // Built after the brain because Manual Add asks it to search the web
    // when the boards and the pasted page come up empty.
    JobScout jobScout(jobPostingRepository, postingSourceRepository,
                      jobSourceRoster, followedEmployerRoster,
                      jobSearchProfile, aiBrain,
                      applicationDataFolderPath, databaseFilePath);

    // The board. Loaded now so every screen that asks "is this job already on
    // my board?" gets a correct answer from the first frame.
    JobPipelines jobPipelines(jobApplicationRepository, jobPostingRepository,
                              postingSourceRepository);
    jobPipelines.loadFromDatabase();

    // Asks employers whether the jobs on the board are still listed. Only
    // when the user presses the button; never on its own.
    PostingStillListedChecker postingStillListedChecker(jobPipelines, jobPostingRepository);

    BrainChatSession brainChatSession(aiBrain);

    // "Moonlight, clean this up!" on ProDocs. Only runs when pressed.
    MoonlightDocumentCleanup moonlightDocumentCleanup(aiBrain, proDocsIntake);

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

    // Crushing a job also looks for the same job on the employer's own board,
    // and adds it as another place to apply (BACKLOG item 13). The board
    // redraws when one is found, so the card and Staging show it.
    QObject::connect(&jobPipelines, &JobPipelines::jobWasCrushed, &jobScout,
                     [&jobPipelines, &jobScout](qint64 jobApplicationId) {
        for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
            if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
                jobScout.lookForOtherPlacesToApply(targetedJob.posting.jobPostingId);
                return;
            }
        }
    });
    QObject::connect(&jobScout, &JobScout::placesToApplyChanged,
                     &jobPipelines, &JobPipelines::loadFromDatabase);

    // The Active Jobs folder: one folder per job, each with a JobQuest.txt
    // that any outside AI can write. Started after the board is loaded and
    // after Staging listens for new cards, because reading the folder can put
    // jobs on the board.
    ActiveJobsFolder activeJobsFolder(jobPipelines, jobPostingRepository,
                                      activeJobFolderRepository);
    activeJobsFolder.loadFromSettings();
    activeJobsFolder.startKeepingTheBoardAndFolderTheSame();

    // The connection AI apps use to work Job Crush directly (see
    // tools/ai-app-bridge). Job Crush stays the only program that writes its
    // database: the AI asks, and the same Logic classes the screens use do
    // the work.
    AiAppRequests aiAppRequests(jobPipelines, jobPostingRepository, activeJobsFolder);
    AiAppConnection aiAppConnection(aiAppRequests);
    aiAppConnection.loadFromSettings();

    // --- ViewModel layer ----------------------------------------------------
    BrainChatConversationViewModel brainChatConversationViewModel(
        brainChatSession, aiBrain);
    AiCredentialRosterViewModel aiCredentialRosterViewModel(aiCredentialRoster);
    AppPreferencesViewModel appPreferencesViewModel(appPreferences, aiBrainSoul);
    SelectedBrainConnectionViewModel selectedBrainConnectionViewModel(aiBrain);
    DiscoveredJobListViewModel discoveredJobListViewModel(jobScout, jobPipelines);
    JobPipelineBoardViewModel jobPipelineBoardViewModel(jobPipelines,
                                                        postingStillListedChecker,
                                                        activeJobsFolder,
                                                        jobScout);
    ActiveJobsFolderViewModel activeJobsFolderViewModel(activeJobsFolder);
    AiAppConnectionViewModel aiAppConnectionViewModel(aiAppConnection);
    JobSourceRosterViewModel jobSourceRosterViewModel(jobSourceRoster);
    FollowedEmployerListViewModel followedEmployerListViewModel(followedEmployerRoster);
    JobSearchProfileViewModel jobSearchProfileViewModel(jobSearchProfile);
    ProfessionalDocumentListViewModel professionalDocumentListViewModel(
        professionalDocumentRepository, proDocsIntake, appPreferences,
        moonlightDocumentCleanup, applicationDataFolderPath);
    WorkExperienceListViewModel workExperienceListViewModel(
        careerHistoryRepository, proDocsIntake);
    EducationListViewModel educationListViewModel(careerHistoryRepository, proDocsIntake);
    StagedJobListViewModel stagedJobListViewModel(jobPipelines, stagedDocumentRepository,
                                                  stagingWorkbench);
    StagingPacketViewModel stagingPacketViewModel(stagingWorkbench, stagedDocumentRepository,
                                                  jobPipelines, appPreferences,
                                                  activeJobsFolder,
                                                  applicationDataFolderPath);
    JobSearchStatsViewModel jobSearchStatsViewModel(jobSearchStatistics, stagingWorkbench);

    // --- View layer: the QML engine ----------------------------------------
    // If Main.qml fails to load, quit with an error code instead of running
    // with no window. Queued so the exit happens once the event loop runs.
    QQmlApplicationEngine qmlEngine;
    QObject::connect(
        &qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // Hands each viewmodel to Main.qml as a required property. The names
    // must match the property names declared in Main.qml.
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
        { QStringLiteral("activeJobsFolderViewModel"),
          QVariant::fromValue(&activeJobsFolderViewModel) },
        { QStringLiteral("aiAppConnectionViewModel"),
          QVariant::fromValue(&aiAppConnectionViewModel) },
    });

    qmlEngine.loadFromModule("JobCrush", "Main");

    return application.exec();
}
