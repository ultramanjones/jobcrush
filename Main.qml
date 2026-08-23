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
    required property var professionalDocumentListViewModel
    required property var workExperienceListViewModel
    required property var educationListViewModel

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

    ProDocsPage {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: navigationSidebar.right
        anchors.leftMargin: 1
        anchors.right: parent.right
        visible: applicationWindow.currentPageName === "proDocs"
        professionalDocumentListViewModel: applicationWindow.professionalDocumentListViewModel
        workExperienceListViewModel: applicationWindow.workExperienceListViewModel
        educationListViewModel: applicationWindow.educationListViewModel
    }

    // ------------------------------------------------------------------
    // The drop basket
    //
    // Deliberately on the window rather than on ProDocs. Moonlight tells
    // people to drop their resume "right here", and that has to be true from
    // whatever screen they happen to be looking at — the one time someone
    // drops a file on the chat and nothing happens, they conclude the whole
    // feature is broken and never try again.
    // ------------------------------------------------------------------
    DropArea {
        id: documentDropArea

        anchors.fill: parent

        onDropped: function(dropEvent) {
            if (!dropEvent.hasUrls) {
                return
            }
            let droppedFileUrls = []
            for (let urlIndex = 0; urlIndex < dropEvent.urls.length; ++urlIndex) {
                droppedFileUrls.push(dropEvent.urls[urlIndex].toString())
            }
            applicationWindow.professionalDocumentListViewModel
                .acceptDroppedFiles(droppedFileUrls)
        }
    }

    // The basket itself: only on screen while something is actually being
    // dragged over the window, so it is never in the way.
    Rectangle {
        anchors.fill: parent
        visible: documentDropArea.containsDrag
        color: JobCrushTheme.appBackgroundColor
        opacity: 0.94
        z: 900

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(520, parent.width - 80)
            height: dropBasketColumn.implicitHeight + 64
            radius: 16
            color: JobCrushTheme.panelBackgroundColor
            border.color: JobCrushTheme.accentColor
            border.width: 3

            Column {
                id: dropBasketColumn
                anchors.centerIn: parent
                width: parent.width - 64
                spacing: 10

                Text {
                    width: parent.width
                    text: "Drop it right here"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.titleFontSize
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: "Job Crush will file it under ProDocs and read what it can. "
                          + "It keeps its own copy — yours stays exactly where it is."
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: "Resumes, cover letters, transcripts, certificates, pictures"
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // What became of the last drop, in the app's own words — including the
    // parts that are bad news. It waits to be dismissed rather than fading:
    // "that PDF is a scan and has no text in it" is precisely the sentence a
    // timer must not be allowed to take away.
    Rectangle {
        id: dropOutcomeNotice

        readonly property string outcomeText:
            applicationWindow.professionalDocumentListViewModel.lastDropOutcomeText

        property bool dismissed: false

        onOutcomeTextChanged: dismissed = false

        visible: outcomeText.length > 0 && !dismissed
        z: 880

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(680, parent.width - 80)
        height: dropOutcomeLabel.implicitHeight + 28
        radius: 10
        color: JobCrushTheme.cardBackgroundColor
        border.color: JobCrushTheme.accentColor
        border.width: 1

        Text {
            id: dropOutcomeLabel
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: dropOutcomeDismissLabel.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: dropOutcomeNotice.outcomeText
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            wrapMode: Text.Wrap
        }

        Text {
            id: dropOutcomeDismissLabel
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "×"
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.titleFontSize

            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.PointingHandCursor
                onClicked: dropOutcomeNotice.dismissed = true
            }
        }
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
