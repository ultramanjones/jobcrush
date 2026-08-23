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
    src/model/JobApplicationRepository.h
    src/model/JobApplicationRepository.cpp
    src/model/ProfessionalDocumentRepository.h
    src/model/ProfessionalDocumentRepository.cpp
    src/model/WorkExperience.h
    src/model/EducationRecord.h
    src/model/CareerHistoryRepository.h
    src/model/CareerHistoryRepository.cpp

    # ModelView layer — the whole back end
    src/modelview/AppPreferences.h
    src/modelview/AppPreferences.cpp
    src/modelview/aibrain/AiCredential.h
    src/modelview/aibrain/AiBrainReply.h
    src/modelview/aibrain/AiBrainProvider.h
    src/modelview/aibrain/AnthropicApiProvider.h
    src/modelview/aibrain/AnthropicApiProvider.cpp
    src/modelview/aibrain/OpenRouterApiProvider.h
    src/modelview/aibrain/OpenRouterApiProvider.cpp
    src/modelview/aibrain/GeminiApiProvider.h
    src/modelview/aibrain/GeminiApiProvider.cpp
    src/modelview/aibrain/AiCredentialRoster.h
    src/modelview/aibrain/AiCredentialRoster.cpp
    src/modelview/aibrain/AiBrainSoul.h
    src/modelview/aibrain/AiBrainSoul.cpp
    src/modelview/aibrain/AiBrain.h
    src/modelview/aibrain/AiBrain.cpp
    src/modelview/brainchat/BrainChatMessage.h
    src/modelview/brainchat/BrainChatSession.h
    src/modelview/brainchat/BrainChatSession.cpp
    src/modelview/jobscout/JobSourceDescriptor.h
    src/modelview/jobscout/KnownWorkLocations.h
    src/modelview/jobscout/JobPostingTextCleanup.h
    src/modelview/jobscout/JobScoutReply.h
    src/modelview/jobscout/JobSourceProvider.h
    src/modelview/jobscout/RemotiveJobSource.h
    src/modelview/jobscout/RemotiveJobSource.cpp
    src/modelview/jobscout/ArbeitnowJobSource.h
    src/modelview/jobscout/ArbeitnowJobSource.cpp
    src/modelview/jobscout/JobSearchProfile.h
    src/modelview/jobscout/JobSearchProfile.cpp
    src/modelview/jobscout/JobSourceRoster.h
    src/modelview/jobscout/JobSourceRoster.cpp
    src/modelview/jobscout/ProspectScorer.h
    src/modelview/jobscout/ProspectScorer.cpp
    src/modelview/jobscout/JobScout.h
    src/modelview/jobscout/JobScout.cpp
    src/modelview/prodocs/DocumentKind.h
    src/modelview/prodocs/DroppedFileTypes.h
    src/modelview/prodocs/DocumentTextExtractor.h
    src/modelview/prodocs/DocumentTextExtractor.cpp
    src/modelview/prodocs/DroppedFileClassifier.h
    src/modelview/prodocs/DroppedFileClassifier.cpp
    src/modelview/prodocs/DeflateDecompressor.h
    src/modelview/prodocs/DeflateDecompressor.cpp
    src/modelview/prodocs/ZipArchiveReader.h
    src/modelview/prodocs/ZipArchiveReader.cpp
    src/modelview/prodocs/WordDocumentReader.h
    src/modelview/prodocs/WordDocumentReader.cpp
    src/modelview/pipelines/JobPipelines.h
    src/modelview/pipelines/JobPipelines.cpp
    src/modelview/prodocs/PlainTextNormalizer.h
    src/modelview/prodocs/ResumeInsightParser.h
    src/modelview/prodocs/ResumeInsightParser.cpp
    src/modelview/prodocs/ProDocsIntake.h
    src/modelview/prodocs/ProDocsIntake.cpp

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
    src/viewmodel/JobSearchProfileViewModel.h
    src/viewmodel/JobSearchProfileViewModel.cpp
    src/viewmodel/ProfessionalDocumentListViewModel.h
    src/viewmodel/ProfessionalDocumentListViewModel.cpp
    src/viewmodel/WorkExperienceListViewModel.h
    src/viewmodel/WorkExperienceListViewModel.cpp
    src/viewmodel/EducationListViewModel.h
    src/viewmodel/EducationListViewModel.cpp
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
set_source_files_properties(JobCrushTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(jobcrush
    URI JobCrush
    VERSION 1.0
    QML_FILES
        Main.qml
        JobCrushTheme.qml
        NavigationSidebar.qml
        BrainChatPage.qml
        DiscoveriesPage.qml
        JobPipelinesPage.qml
        ProDocsPage.qml
        SettingsPage.qml
        InlineEditField.qml
        TokenEntryField.qml
        VerticalScrollBar.qml
)

# The icon the app carries at RUNTIME — the one Windows shows in the taskbar
# and alt-tab while jobcrush is running, and the one every other desktop uses.
qt_add_resources(jobcrush "applicationIcon"
    PREFIX "/"
    FILES
        assets/jobcrush-icon.png
)

target_link_libraries(jobcrush PRIVATE Qt6::Quick Qt6::Sql Qt6::Network Qt6::Pdf)
