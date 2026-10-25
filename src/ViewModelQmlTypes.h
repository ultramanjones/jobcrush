#pragma once

// ViewModelQmlTypes
//
// Tells QML the name of each viewmodel class, so the pages can declare
// typed properties such as
//
//     property DiscoveredJobListViewModel discoveredJobListViewModel
//
// instead of "property var". With a real type, qmllint and Qt Creator check
// every property and call on the viewmodel, and the Qt Quick compiler can
// turn the bindings into C++.
//
// Why this file exists instead of a QML_ELEMENT line in each viewmodel: the
// viewmodel layer knows nothing about QML, and that is a rule (see
// Cleere_Programming_Best_Practices.md). So the viewmodels are registered
// from the outside, here, with QML_FOREIGN. This file sits beside main.cpp,
// the one place that wires the layers together.
//
// Each entry is uncreatable. QML never builds a viewmodel. main.cpp builds
// every one of them and hands them to Main.qml.
//
// To add a viewmodel: include its header and copy one entry below. moc
// reads each entry as written, so they cannot be made by a macro.

#include <QtQml/qqml.h>

#include "viewmodel/ActiveJobsFolderViewModel.h"
#include "viewmodel/AiAppConnectionViewModel.h"
#include "viewmodel/AiCredentialRosterViewModel.h"
#include "viewmodel/AppPreferencesViewModel.h"
#include "viewmodel/BrainChatConversationViewModel.h"
#include "viewmodel/DiscoveredJobListViewModel.h"
#include "viewmodel/EducationListViewModel.h"
#include "viewmodel/FollowedEmployerListViewModel.h"
#include "viewmodel/JobPipelineBoardViewModel.h"
#include "viewmodel/JobSearchProfileViewModel.h"
#include "viewmodel/JobSearchStatsViewModel.h"
#include "viewmodel/JobSourceRosterViewModel.h"
#include "viewmodel/ProfessionalDocumentListViewModel.h"
#include "viewmodel/SelectedBrainConnectionViewModel.h"
#include "viewmodel/StagedJobListViewModel.h"
#include "viewmodel/StagingPacketViewModel.h"
#include "viewmodel/WorkExperienceListViewModel.h"

struct ActiveJobsFolderViewModelForQml {
    Q_GADGET
    QML_FOREIGN(ActiveJobsFolderViewModel)
    QML_NAMED_ELEMENT(ActiveJobsFolderViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct AiAppConnectionViewModelForQml {
    Q_GADGET
    QML_FOREIGN(AiAppConnectionViewModel)
    QML_NAMED_ELEMENT(AiAppConnectionViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct AiCredentialRosterViewModelForQml {
    Q_GADGET
    QML_FOREIGN(AiCredentialRosterViewModel)
    QML_NAMED_ELEMENT(AiCredentialRosterViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct AppPreferencesViewModelForQml {
    Q_GADGET
    QML_FOREIGN(AppPreferencesViewModel)
    QML_NAMED_ELEMENT(AppPreferencesViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct BrainChatConversationViewModelForQml {
    Q_GADGET
    QML_FOREIGN(BrainChatConversationViewModel)
    QML_NAMED_ELEMENT(BrainChatConversationViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct DiscoveredJobListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(DiscoveredJobListViewModel)
    QML_NAMED_ELEMENT(DiscoveredJobListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct EducationListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(EducationListViewModel)
    QML_NAMED_ELEMENT(EducationListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct FollowedEmployerListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(FollowedEmployerListViewModel)
    QML_NAMED_ELEMENT(FollowedEmployerListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct JobPipelineBoardViewModelForQml {
    Q_GADGET
    QML_FOREIGN(JobPipelineBoardViewModel)
    QML_NAMED_ELEMENT(JobPipelineBoardViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct JobSearchProfileViewModelForQml {
    Q_GADGET
    QML_FOREIGN(JobSearchProfileViewModel)
    QML_NAMED_ELEMENT(JobSearchProfileViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct JobSearchStatsViewModelForQml {
    Q_GADGET
    QML_FOREIGN(JobSearchStatsViewModel)
    QML_NAMED_ELEMENT(JobSearchStatsViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct JobSourceRosterViewModelForQml {
    Q_GADGET
    QML_FOREIGN(JobSourceRosterViewModel)
    QML_NAMED_ELEMENT(JobSourceRosterViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct ProfessionalDocumentListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(ProfessionalDocumentListViewModel)
    QML_NAMED_ELEMENT(ProfessionalDocumentListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct SelectedBrainConnectionViewModelForQml {
    Q_GADGET
    QML_FOREIGN(SelectedBrainConnectionViewModel)
    QML_NAMED_ELEMENT(SelectedBrainConnectionViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct StagedJobListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(StagedJobListViewModel)
    QML_NAMED_ELEMENT(StagedJobListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct StagingPacketViewModelForQml {
    Q_GADGET
    QML_FOREIGN(StagingPacketViewModel)
    QML_NAMED_ELEMENT(StagingPacketViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};

struct WorkExperienceListViewModelForQml {
    Q_GADGET
    QML_FOREIGN(WorkExperienceListViewModel)
    QML_NAMED_ELEMENT(WorkExperienceListViewModel)
    QML_UNCREATABLE("Built in main.cpp and handed to Main.qml.")
};
