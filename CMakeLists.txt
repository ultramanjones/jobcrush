cmake_minimum_required(VERSION 3.21)

project(jobcrush VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Sql Network Pdf)
qt_standard_project_setup(REQUIRES 6.5)

# The Windows executable icon, compiled in as a resource. Guarded because a
# .rc file means nothing to any other platform.
if (WIN32)
    set(jobCrushWindowsIconResource assets/jobcrush.rc)
endif()

qt_add_executable(jobcrush
    src/main.cpp
    ${jobCrushWindowsIconResource}

    # Model layer — close to the metal
    src/model/PipelineStage.h
    src/model/JobPosting.h
    src/model/JobApplication.h
    src/model/ProfessionalDocument.h
    src/model/JobCrushDatabase.h
    src/model/JobCrushDatabase.cpp
    src/model/JobPostingRepository.h
    src/model/JobPostingRepository.cpp
    src/model/PostingSource.h
    src/model/PostingSourceRepository.h
    src/model/PostingSourceRepository.cpp
    src/model/JobApplicationRepository.h
    src/model/JobApplicationRepository.cpp
    src/model/ProfessionalDocumentRepository.h
    src/model/ProfessionalDocumentRepository.cpp
    src/model/WorkExperience.h
    src/model/EducationRecord.h
    src/model/CareerHistoryRepository.h
    src/model/CareerHistoryRepository.cpp
    src/model/StagedDocument.h
    src/model/StagedDocumentRepository.h
    src/model/StagedDocumentRepository.cpp
    src/model/ActiveJobFolder.h
    src/model/ActiveJobFolderRepository.h
    src/model/ActiveJobFolderRepository.cpp

    # Logic layer — the whole back end
    src/logic/AppPreferences.h
    src/logic/AppPreferences.cpp
    src/logic/aibrain/AiCredential.h
    src/logic/aibrain/AiBrainReply.h
    src/logic/aibrain/AiBrainProvider.h
    src/logic/aibrain/AnthropicApiProvider.h
    src/logic/aibrain/AnthropicApiProvider.cpp
    src/logic/aibrain/OpenRouterApiProvider.h
    src/logic/aibrain/OpenRouterApiProvider.cpp
    src/logic/aibrain/GeminiApiProvider.h
    src/logic/aibrain/GeminiApiProvider.cpp
    src/logic/aibrain/OpenAiStyleStream.h
    src/logic/aibrain/OpenAiStyleStream.cpp
    src/logic/aibrain/OpenAiApiProvider.h
    src/logic/aibrain/OpenAiApiProvider.cpp
    src/logic/aibrain/OllamaProvider.h
    src/logic/aibrain/OllamaProvider.cpp
    src/logic/aibrain/AiCredentialRoster.h
    src/logic/aibrain/AiCredentialRoster.cpp
    src/logic/aibrain/AiBrainSoul.h
    src/logic/aibrain/AiBrainSoul.cpp
    src/logic/aibrain/AiBrain.h
    src/logic/aibrain/AiBrain.cpp
    src/logic/brainchat/BrainChatMessage.h
    src/logic/brainchat/MoonlightHints.h
    src/logic/brainchat/BrainChatSession.h
    src/logic/brainchat/BrainChatSession.cpp
    src/logic/jobscout/JobSourceDescriptor.h
    src/logic/jobscout/KnownWorkLocations.h
    src/logic/jobscout/JobPostingTextCleanup.h
    src/logic/jobscout/JobScoutReply.h
    src/logic/jobscout/JobSourceProvider.h
    src/logic/jobscout/JobLead.h
    src/logic/jobscout/AtsBoardIdentity.h
    src/logic/jobscout/AtsBoardDetector.h
    src/logic/jobscout/UrlCanonicalizer.h
    src/logic/jobscout/DuplicateJobDetector.h
    src/logic/jobscout/EmployerBoardProvider.h
    src/logic/jobscout/EmployerBoardHttp.h
    src/logic/jobscout/GreenhouseBoardSource.h
    src/logic/jobscout/GreenhouseBoardSource.cpp
    src/logic/jobscout/LeverBoardSource.h
    src/logic/jobscout/LeverBoardSource.cpp
    src/logic/jobscout/AshbyBoardSource.h
    src/logic/jobscout/AshbyBoardSource.cpp
    src/logic/jobscout/WorkdayBoardSource.h
    src/logic/jobscout/WorkdayBoardSource.cpp
    src/logic/jobscout/TaleoBoardSource.h
    src/logic/jobscout/TaleoBoardSource.cpp
    src/logic/jobscout/SmartRecruitersBoardSource.h
    src/logic/jobscout/SmartRecruitersBoardSource.cpp
    src/logic/jobscout/EmployerTenantGuesser.h
    src/logic/jobscout/CanonicalPostingResolver.h
    src/logic/jobscout/CanonicalPostingResolver.cpp
    src/logic/jobscout/PostedJobPageReader.h
    src/logic/jobscout/PostedJobPageReader.cpp
    src/logic/jobscout/BrainJobFinder.h
    src/logic/jobscout/BrainJobFinder.cpp
    src/logic/jobscout/PastedTextJobReader.h
    src/logic/jobscout/PastedTextJobReader.cpp
    src/logic/jobscout/RemotiveJobSource.h
    src/logic/jobscout/RemotiveJobSource.cpp
    src/logic/jobscout/ArbeitnowJobSource.h
    src/logic/jobscout/ArbeitnowJobSource.cpp
    src/logic/jobscout/JobicyJobSource.h
    src/logic/jobscout/JobicyJobSource.cpp
    src/logic/jobscout/UsaJobsJobSource.h
    src/logic/jobscout/UsaJobsJobSource.cpp
    src/logic/jobscout/FollowedEmployer.h
    src/logic/jobscout/CareerPreset.h
    src/logic/jobscout/CareerPreset.cpp
    src/logic/jobscout/ClearanceDetector.h
    src/logic/jobscout/CompanyList.h
    src/logic/jobscout/CompanyList.cpp
    src/logic/jobscout/FollowedEmployerRoster.h
    src/logic/jobscout/FollowedEmployerRoster.cpp
    src/logic/jobscout/FollowedEmployerJobSource.h
    src/logic/jobscout/FollowedEmployerJobSource.cpp
    src/logic/jobscout/JobSearchProfile.h
    src/logic/jobscout/JobSearchProfile.cpp
    src/logic/jobscout/JobSourceRoster.h
    src/logic/jobscout/JobSourceRoster.cpp
    src/logic/jobscout/ProspectScorer.h
    src/logic/jobscout/ProspectScorer.cpp
    src/logic/jobscout/DiscoveryWorker.h
    src/logic/jobscout/DiscoveryWorker.cpp
    src/logic/jobscout/JobScout.h
    src/logic/jobscout/JobScout.cpp
    src/logic/prodocs/DocumentKind.h
    src/logic/prodocs/DroppedFileTypes.h
    src/logic/prodocs/DocumentTextExtractor.h
    src/logic/prodocs/DocumentTextExtractor.cpp
    src/logic/prodocs/DroppedFileClassifier.h
    src/logic/prodocs/DroppedFileClassifier.cpp
    src/logic/prodocs/DeflateDecompressor.h
    src/logic/prodocs/DeflateDecompressor.cpp
    src/logic/prodocs/ZipArchiveReader.h
    src/logic/prodocs/ZipArchiveReader.cpp
    src/logic/prodocs/WordDocumentReader.h
    src/logic/prodocs/WordDocumentReader.cpp
    src/logic/pipelines/JobPipelines.h
    src/logic/pipelines/JobPipelines.cpp
    src/logic/pipelines/TypedDateReader.h
    src/logic/pipelines/TypedDateReader.cpp
    src/logic/pipelines/PostingGoneConfirmer.h
    src/logic/pipelines/PostingGoneConfirmer.cpp
    src/logic/pipelines/PostingStillListedChecker.h
    src/logic/pipelines/PostingStillListedChecker.cpp
    src/logic/prodocs/PlainTextNormalizer.h
    src/logic/prodocs/InstitutionWords.h
    src/logic/prodocs/ResumeInsightParser.h
    src/logic/prodocs/ResumeInsightParser.cpp
    src/logic/prodocs/AcademicTranscriptReader.h
    src/logic/prodocs/AcademicTranscriptReader.cpp
    src/logic/prodocs/MoonlightDocumentCleanup.h
    src/logic/prodocs/MoonlightDocumentCleanup.cpp
    src/logic/prodocs/ProDocsIntake.h
    src/logic/prodocs/ProDocsIntake.cpp
    src/logic/tasks/AiBrainTask.h
    src/logic/tasks/PostingRequirementReader.h
    src/logic/tasks/TaskInstructionWriter.h
    src/logic/tasks/TaskInstructionWriter.cpp
    src/logic/tasks/TaskReplyReader.h
    src/logic/tasks/TaskReplyReader.cpp
    src/logic/tasks/StagingWorkbench.h
    src/logic/tasks/StagingWorkbench.cpp
    src/logic/exporting/ExportFormat.h
    src/logic/exporting/MarkdownDocumentReader.h
    src/logic/exporting/ZipArchiveWriter.h
    src/logic/exporting/FormattedDocumentWriter.h
    src/logic/exporting/FormattedDocumentWriter.cpp
    src/logic/exporting/DocumentConverter.h
    src/logic/exporting/DocumentConverter.cpp
    src/logic/exporting/ExportFolder.h
    src/logic/exporting/PacketExporter.h
    src/logic/exporting/PacketExporter.cpp
    src/logic/stats/JobSearchStatistics.h
    src/logic/stats/JobSearchStatistics.cpp
    src/logic/activejobs/JobQuestFile.h
    src/logic/activejobs/JobQuestFile.cpp
    src/logic/activejobs/OlderJobFolderReader.h
    src/logic/activejobs/OlderJobFolderReader.cpp
    src/logic/activejobs/AiBuddyInstructions.h
    src/logic/activejobs/ActiveJobsFolder.h
    src/logic/activejobs/ActiveJobsFolder.cpp
    src/logic/aiapps/AiAppTools.h
    src/logic/aiapps/AiAppRequests.h
    src/logic/aiapps/AiAppRequests.cpp
    src/logic/aiapps/AiAppConnection.h
    src/logic/aiapps/AiAppConnection.cpp

    # ViewModel layer — translation and organization only
    src/viewmodel/BrainChatConversationViewModel.h
    src/viewmodel/BrainChatConversationViewModel.cpp
    src/viewmodel/AiCredentialRosterViewModel.h
    src/viewmodel/AiCredentialRosterViewModel.cpp
    src/viewmodel/AppPreferencesViewModel.h
    src/viewmodel/AppPreferencesViewModel.cpp
    src/viewmodel/SelectedBrainConnectionViewModel.h
    src/viewmodel/SelectedBrainConnectionViewModel.cpp
    src/viewmodel/JobPipelineBoardViewModel.h
    src/viewmodel/JobPipelineBoardViewModel.cpp
    src/viewmodel/DiscoveredJobListViewModel.h
    src/viewmodel/DiscoveredJobListViewModel.cpp
    src/viewmodel/JobSourceRosterViewModel.h
    src/viewmodel/JobSourceRosterViewModel.cpp
    src/viewmodel/FollowedEmployerListViewModel.h
    src/viewmodel/FollowedEmployerListViewModel.cpp
    src/viewmodel/JobSearchProfileViewModel.h
    src/viewmodel/JobSearchProfileViewModel.cpp
    src/viewmodel/ProfessionalDocumentListViewModel.h
    src/viewmodel/ProfessionalDocumentListViewModel.cpp
    src/viewmodel/WorkExperienceListViewModel.h
    src/viewmodel/WorkExperienceListViewModel.cpp
    src/viewmodel/EducationListViewModel.h
    src/viewmodel/EducationListViewModel.cpp
    src/viewmodel/StagedJobListViewModel.h
    src/viewmodel/StagedJobListViewModel.cpp
    src/viewmodel/StagingPacketViewModel.h
    src/viewmodel/StagingPacketViewModel.cpp
    src/viewmodel/JobSearchStatsViewModel.h
    src/viewmodel/JobSearchStatsViewModel.cpp
    src/viewmodel/ActiveJobsFolderViewModel.h
    src/viewmodel/ActiveJobsFolderViewModel.cpp
    src/viewmodel/AiAppConnectionViewModel.h
    src/viewmodel/AiAppConnectionViewModel.cpp
)

target_include_directories(jobcrush PRIVATE src)

# MinGW/Windows only: Windows COFF object files cap at 32,768 sections, and a
# debug build of mocs_compilation.cpp — every moc file in the project compiled
# as one translation unit — walks straight into that ceiling once the class
# count grows. The failure gives no source line, just "Error 1", so it reads
# like a code bug when it is a file-format limit.
#
# -Wa,-mbig-obj tells the assembler to emit the extended COFF format that has
# no such cap. Costs nothing, changes no behavior, and MSVC and every Unix
# toolchain are unaffected because they never had the limit.
if (MINGW)
    target_compile_options(jobcrush PRIVATE -Wa,-mbig-obj)
endif()

# The theme singleton must be declared as such before qt_add_qml_module.
set_source_files_properties(src/view/JobCrushTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(jobcrush
    URI JobCrush
    VERSION 1.0
    QML_FILES
        src/view/Main.qml
        src/view/JobCrushTheme.qml
        src/view/ThemePalette.qml
        src/view/ThemeClassic.qml
        src/view/ThemeGrayscale.qml
        src/view/ThemeFruitLoops.qml
        src/view/ThemeAmish.qml
        src/view/ThemeTraceOn.qml
        src/view/ThemeTraceOnTwo.qml
        src/view/ThemeWraithWranglers.qml
        src/view/NavigationSidebar.qml
        src/view/BrainChatPage.qml
        src/view/DiscoveriesPage.qml
        src/view/JobPipelinesPage.qml
        src/view/ProDocsPage.qml
        src/view/StagingPage.qml
        src/view/StatsPage.qml
        src/view/SettingsPage.qml
        src/view/AddOneJobPanel.qml
        src/view/InlineEditField.qml
        src/view/TokenEntryField.qml
        src/view/VerticalScrollBar.qml
        src/view/GlowHalo.qml
)

# Lists of companies, kept as data so they can change without changing code.
qt_add_resources(jobcrush "companyLists"
    PREFIX "/"
    FILES
        data/company-lists/cpp-qt-medtech-tech.json
        data/career-presets/cpp-qt.json
)

# The icon the app carries at RUNTIME — the one Windows shows in the taskbar
# and alt-tab while jobcrush is running, and the one every other desktop uses.
qt_add_resources(jobcrush "applicationIcon"
    PREFIX "/"
    FILES
        assets/jobcrush-icon.png
)

target_link_libraries(jobcrush PRIVATE Qt6::Quick Qt6::Sql Qt6::Network Qt6::Pdf)

# The bridge an AI app starts to talk to Job Crush (see
# tools/ai-app-bridge/main.cpp). A console program on purpose: AI apps talk to
# it through its standard input and output, which a window program on Windows
# does not reliably have. It sits next to jobcrush.exe, and Settings shows
# the AI app where to find it.
qt_add_executable(jobcrush-ai-bridge
    tools/ai-app-bridge/main.cpp
    src/logic/aiapps/AiAppTools.h
)
set_target_properties(jobcrush-ai-bridge PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)
target_include_directories(jobcrush-ai-bridge PRIVATE src)
target_link_libraries(jobcrush-ai-bridge PRIVATE Qt6::Core Qt6::Network)

# --- Installing ------------------------------------------------------------
#
# "cmake --install" puts Job Crush, the AI app bridge, and everything they need
# to run into one folder. On Windows, Qt's own deployment step (windeployqt)
# copies in the Qt files, the QML pieces the screens use, and the database
# driver. That folder is what the Windows installer packs up; see
# tools/installer/. A stranger's computer has no Qt, so nothing may be left out.
include(GNUInstallDirs)
install(TARGETS jobcrush jobcrush-ai-bridge
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_generate_deploy_qml_app_script(
    TARGET jobcrush
    OUTPUT_SCRIPT jobCrushDeployScript
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${jobCrushDeployScript})

# The Microsoft C++ runtime, when built with Visual Studio. A computer that has
# never had it installed cannot start Job Crush without these files.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)

# Tests. On by default, so the claim that the core runs without a window is
# checked on every build. Turn off with -DJOBCRUSH_BUILD_TESTS=OFF.
option(JOBCRUSH_BUILD_TESTS "Build the Job Crush tests" ON)
if (JOBCRUSH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
