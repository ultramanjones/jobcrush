import QtQuick

// NavigationSidebar
//
// The left-hand rail — Job Crush's only navigation (top menus are played
// out). Fixed destinations; entries whose phases haven't landed yet render
// dimmed with their phase number, so the roadmap is visible in the app
// itself. Pure view: emits the chosen page name upward, holds no logic.
Rectangle {
    id: sidebar

    // The page the app is currently showing (set from Main).
    property string currentPageName: "brainChat"

    // Emitted when the user picks an enabled destination.
    signal pageRequested(string pageName)

    width: 220
    color: JobCrushTheme.sidebarBackgroundColor

    // Destination list: name is the internal page id; phaseTag marks
    // not-yet-built pages ("" = live now).
    readonly property var navigationEntries: [
        { pageName: "pipelines",   displayLabel: "Job Pipelines", phaseTag: "" },
        { pageName: "discoveries", displayLabel: "Discoveries", phaseTag: "" },
        { pageName: "proDocs",     displayLabel: "ProDocs",     phaseTag: "" },
        { pageName: "staging",     displayLabel: "Staging",     phaseTag: "Phase 5" },
        { pageName: "stats",       displayLabel: "Stats",       phaseTag: "Phase 7" },
        { pageName: "brainChat",   displayLabel: "Brain Chat",  phaseTag: "" },
        { pageName: "settings",    displayLabel: "Settings",    phaseTag: "" }
    ]

    Column {
        anchors.top: parent.top
        anchors.topMargin: 24
        anchors.left: parent.left
        anchors.right: parent.right

        // Wordmark corner.
        Text {
            text: "JOB CRUSH"
            color: JobCrushTheme.sidebarWordmarkColor
            font.pixelSize: 18
            font.weight: Font.Bold
            font.letterSpacing: 2
            leftPadding: 20
            bottomPadding: 28
        }

        Repeater {
            model: sidebar.navigationEntries

            delegate: Rectangle {
                id: navigationRow

                required property var modelData

                readonly property bool isEnabled: modelData.phaseTag === ""
                readonly property bool isCurrent: sidebar.currentPageName === modelData.pageName

                width: sidebar.width
                height: 44
                color: isCurrent ? JobCrushTheme.sidebarSelectedRowColor : "transparent"

                // Current-page indicator: a thin accent bar, not a highlight
                // shout.
                Rectangle {
                    width: 3
                    height: parent.height
                    color: JobCrushTheme.accentColor
                    visible: navigationRow.isCurrent
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    text: navigationRow.modelData.displayLabel
                    color: navigationRow.isEnabled
                        ? (navigationRow.isCurrent
                               ? JobCrushTheme.sidebarPrimaryTextColor
                               : JobCrushTheme.sidebarSecondaryTextColor)
                        : JobCrushTheme.sidebarMutedTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    text: navigationRow.modelData.phaseTag
                    visible: !navigationRow.isEnabled
                    color: JobCrushTheme.sidebarMutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: navigationRow.isEnabled
                    cursorShape: navigationRow.isEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: sidebar.pageRequested(navigationRow.modelData.pageName)
                }
            }
        }
    }
}
