import QtQuick

// Main
//
// The application window: the navigation sidebar on the left, the current
// page on the right. All viewmodels arrive from the composition root
// (main.cpp) via initial properties — the view layer creates none of them.
//
// Pages never talk to each other (no crosstalk law): navigation wishes are
// signalled up to here, and this window switches pages.
Window {
    id: applicationWindow

    // Injected by main.cpp.
    required property var brainChatConversationViewModel
    required property var aiCredentialRosterViewModel
    required property var appPreferencesViewModel
    required property var selectedBrainConnectionViewModel
    required property var discoveredJobListViewModel
    required property var jobSourceRosterViewModel
    required property var jobSearchProfileViewModel

    width: 1280
    height: 800
    visible: true
    title: qsTr("Job Crush")
    color: JobCrushTheme.appBackgroundColor

    // The theme singleton follows the preference, instantly.
    Binding {
        target: JobCrushTheme
        property: "activeThemeName"
        value: applicationWindow.appPreferencesViewModel.boardThemeName
    }

    // Which page is showing. Brain Chat is home while the board (Phase 4)
    // doesn't exist yet.
    property string currentPageName: "brainChat"

    NavigationSidebar {
        id: navigationSidebar
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        currentPageName: applicationWindow.currentPageName
        onPageRequested: function(pageName) {
            applicationWindow.currentPageName = pageName
        }
    }

    Rectangle {
        width: 1
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: navigationSidebar.right
        color: JobCrushTheme.hairlineBorderColor
    }

    BrainChatPage {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: navigationSidebar.right
        anchors.leftMargin: 1
        anchors.right: parent.right
        visible: applicationWindow.currentPageName === "brainChat"
        conversationViewModel: applicationWindow.brainChatConversationViewModel
        brainConnectionViewModel: applicationWindow.selectedBrainConnectionViewModel
        onSettingsRequested: applicationWindow.currentPageName = "settings"
    }

    DiscoveriesPage {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: navigationSidebar.right
        anchors.leftMargin: 1
        anchors.right: parent.right
        visible: applicationWindow.currentPageName === "discoveries"
        discoveredJobListViewModel: applicationWindow.discoveredJobListViewModel
        jobSourceRosterViewModel: applicationWindow.jobSourceRosterViewModel
        onSettingsRequested: applicationWindow.currentPageName = "settings"
    }

    SettingsPage {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: navigationSidebar.right
        anchors.leftMargin: 1
        anchors.right: parent.right
        visible: applicationWindow.currentPageName === "settings"
        credentialRosterViewModel: applicationWindow.aiCredentialRosterViewModel
        preferencesViewModel: applicationWindow.appPreferencesViewModel
        brainConnectionViewModel: applicationWindow.selectedBrainConnectionViewModel
        jobSourceRosterViewModel: applicationWindow.jobSourceRosterViewModel
        jobSearchProfileViewModel: applicationWindow.jobSearchProfileViewModel
    }
}
