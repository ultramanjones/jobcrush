import QtQuick
import JobCrush

// NavigationSidebar
//
// The left-hand rail. It is Job Crush's only navigation. There are no top
// menus. The list of pages is fixed. A page that is not built yet shows
// dimmed, with its phase number on the right, so the roadmap shows in the
// app itself. Today every page is built, so none are dimmed.
//
// Pure view: it emits the chosen page name up to Main and holds no logic.
// The page names here must match the names Main.qml checks.
Rectangle {
    id: sidebar

    // The page the app is currently showing (set from Main).
    property string currentPageName: "brainChat"

    // Emitted when the user picks an enabled destination.
    signal pageRequested(string pageName)

    width: 220
    color: JobCrushTheme.sidebarBackgroundColor

    // The pages, top to bottom. pageName is the name Main uses to pick the
    // page. displayLabel is what the user reads. phaseTag marks a page that
    // is not built yet; an empty phaseTag means the page is live.
    readonly property var navigationEntries: [
        { pageName: "pipelines",   displayLabel: "Job Pipelines", phaseTag: "" },
        { pageName: "discoveries", displayLabel: "Discoveries", phaseTag: "" },
        { pageName: "proDocs",     displayLabel: "ProDocs",     phaseTag: "" },
        { pageName: "staging",     displayLabel: "Staging",     phaseTag: "" },
        { pageName: "stats",       displayLabel: "Stats",       phaseTag: "" },
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

                // The current page gets a thin accent bar on its left edge,
                // not a loud highlight.
                Rectangle {
                    width: 3
                    height: parent.height
                    color: JobCrushTheme.accentColor
                    visible: navigationRow.isCurrent

                    GlowHalo {
                        glowColor: JobCrushTheme.accentColor
                    }
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
