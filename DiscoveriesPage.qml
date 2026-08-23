import QtQuick

// DiscoveriesPage
//
// What JobScout found. A tab for Top Prospects — the ranked list across every
// site — and then one tab per job site the user ticked in Settings. Untick a
// site there and its tab disappears from here; one box, one meaning.
//
// THE BOARD IS SACRED: nothing on this page puts a job on the pipeline. That
// only happens when the user hits CRUSH.
//
// Pure view: binds to its viewmodels, emits navigation wishes upward, holds
// no business logic.
Rectangle {
    id: discoveriesPage

    // Injected from Main.
    property var discoveredJobListViewModel
    property var jobSourceRosterViewModel

    // Asks Main to navigate (empty states point the user at Settings).
    signal settingsRequested

    color: JobCrushTheme.appBackgroundColor

    // Which tab is showing. Empty string is Top Prospects; anything else is a
    // job site's storage name. The viewmodel rebuilds its rows off this.
    property string activeTabSourceName: ""

    onActiveTabSourceNameChanged: {
        discoveredJobListViewModel.activeTabSourceName = activeTabSourceName
    }

    readonly property bool showingTopProspects: activeTabSourceName === ""

    // A site the user unticks must not leave a dead tab selected behind it.
    Connections {
        target: discoveriesPage.jobSourceRosterViewModel
        function onEnabledSourcesChanged() {
            if (discoveriesPage.activeTabSourceName === "") {
                return
            }
            const stillEnabled = discoveriesPage.jobSourceRosterViewModel
                .enabledJobSourceTabs.some(function(jobSourceTab) {
                    return jobSourceTab.storageName === discoveriesPage.activeTabSourceName
                })
            if (!stillEnabled) {
                discoveriesPage.activeTabSourceName = ""
            }
        }
    }

    // ------------------------------------------------------------------
    // Header: title, the scout action, and honest progress
    // ------------------------------------------------------------------
    Item {
        id: discoveriesHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "Discoveries"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        // The sweep's running commentary — real counts from real sites.
        // Words that mean something, which is what stands in for a spinner.
        Text {
            id: sweepProgressLabel
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 170
            anchors.right: scoutNowButton.left
            anchors.rightMargin: 20
            // While sweeping: the running counts. Afterwards: whatever went
            // WRONG takes priority over the tidy summary, because a source
            // that failed silently is the one thing the user needs told.
            text: {
                const listViewModel = discoveriesPage.discoveredJobListViewModel
                if (listViewModel.sweepIsRunning) {
                    return listViewModel.sweepProgressText
                }
                if (listViewModel.lastSweepTroubleText.length > 0) {
                    return listViewModel.lastSweepTroubleText
                }
                return listViewModel.lastSweepSummaryText
            }
            color: {
                const listViewModel = discoveriesPage.discoveredJobListViewModel
                if (listViewModel.sweepIsRunning) {
                    return JobCrushTheme.accentColor
                }
                return listViewModel.lastSweepTroubleText.length > 0
                    ? JobCrushTheme.noticeTextColor
                    : JobCrushTheme.mutedTextColor
            }
            font.pixelSize: JobCrushTheme.smallFontSize
            elide: Text.ElideRight

            SequentialAnimation on opacity {
                running: discoveriesPage.discoveredJobListViewModel.sweepIsRunning
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.45; duration: 700 }
                NumberAnimation { from: 0.45; to: 1.0; duration: 700 }
                onRunningChanged: {
                    if (!running) {
                        sweepProgressLabel.opacity = 1.0
                    }
                }
            }
        }

        Rectangle {
            id: scoutNowButton

            readonly property bool sweepIsPossible:
                !discoveriesPage.discoveredJobListViewModel.sweepIsRunning
                && discoveriesPage.jobSourceRosterViewModel.enabledJobSourceTabs.length > 0

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 28
            width: scoutNowButtonLabel.implicitWidth + 32
            height: 36
            radius: 8
            color: sweepIsPossible
                ? (scoutNowMouseArea.containsMouse
                       ? Qt.lighter(JobCrushTheme.callToActionColor, 1.15)
                       : JobCrushTheme.callToActionColor)
                : JobCrushTheme.cardBackgroundColor

            Text {
                id: scoutNowButtonLabel
                anchors.centerIn: parent
                text: discoveriesPage.discoveredJobListViewModel.sweepIsRunning
                    ? "scouting…" : "Scout now"
                color: scoutNowButton.sweepIsPossible
                    ? JobCrushTheme.onAccentTextColor : JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: scoutNowMouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: scoutNowButton.sweepIsPossible
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: discoveriesPage.discoveredJobListViewModel.startSweep()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: JobCrushTheme.hairlineBorderColor
        }
    }

    // ------------------------------------------------------------------
    // Tabs: Top Prospects, then one per ticked site
    // ------------------------------------------------------------------
    Row {
        id: discoveryTabRow
        anchors.top: discoveriesHeader.bottom
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.right: parent.right
        anchors.topMargin: 14
        spacing: 10

        // The ranked list always leads — it is the answer to "what should I
        // look at first?", which is the whole reason this page exists.
        Rectangle {
            readonly property bool isActiveTab: discoveriesPage.showingTopProspects

            width: topProspectsTabLabel.implicitWidth + 28
            height: 32
            radius: 16
            color: isActiveTab ? JobCrushTheme.cardBackgroundColor : "transparent"
            border.color: isActiveTab
                ? JobCrushTheme.callToActionColor : JobCrushTheme.hairlineBorderColor
            // Double weight on the active tab, so the choice reads without
            // relying on color — Grayscale has no accent hue to notice.
            border.width: isActiveTab ? 2 : 1

            Text {
                id: topProspectsTabLabel
                anchors.centerIn: parent
                text: "Top Prospects"
                color: parent.isActiveTab
                    ? JobCrushTheme.primaryTextColor : JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                font.weight: parent.isActiveTab ? Font.DemiBold : Font.Normal
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: discoveriesPage.activeTabSourceName = ""
            }
        }

        Repeater {
            model: discoveriesPage.jobSourceRosterViewModel.enabledJobSourceTabs

            delegate: Rectangle {
                id: jobSourceTab

                required property var modelData

                readonly property bool isActiveTab:
                    discoveriesPage.activeTabSourceName === modelData.storageName

                width: jobSourceTabLabel.implicitWidth + 28
                height: 32
                radius: 16
                color: isActiveTab ? JobCrushTheme.cardBackgroundColor : "transparent"
                border.color: isActiveTab
                    ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
                border.width: isActiveTab ? 2 : 1

                Text {
                    id: jobSourceTabLabel
                    anchors.centerIn: parent
                    text: jobSourceTab.modelData.displayName
                    color: jobSourceTab.isActiveTab
                        ? JobCrushTheme.primaryTextColor : JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: jobSourceTab.isActiveTab ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: discoveriesPage.activeTabSourceName
                        = jobSourceTab.modelData.storageName
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // The rows
    // ------------------------------------------------------------------
    ListView {
        id: discoveredJobListView

        anchors.top: discoveryTabRow.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 28
        // Room for the scroll bar, so a row never slides under it.
        anchors.rightMargin: 46
        anchors.bottomMargin: 20
        spacing: 6
        clip: true

        model: discoveriesPage.discoveredJobListViewModel

        // A mouse wheel notch moves twice what Qt's default would. The stock
        // step is tuned for documents; a list of job rows wants to move.
        WheelHandler {
            id: discoveryWheelHandler

            readonly property real pixelsPerNotch: 120
            readonly property real speedMultiplier: 2.0

            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            onWheel: function(wheelEvent) {
                const scrollableDistance = Math.max(
                    0, discoveredJobListView.contentHeight - discoveredJobListView.height)
                if (scrollableDistance <= 0) {
                    return
                }
                const requestedContentY = discoveredJobListView.contentY
                    - (wheelEvent.angleDelta.y / 120) * pixelsPerNotch * speedMultiplier
                discoveredJobListView.contentY = Math.max(
                    discoveredJobListView.originY,
                    Math.min(discoveredJobListView.originY + scrollableDistance,
                             requestedContentY))
            }
        }

        // Grouped by the day the job was posted, which is how a person
        // actually thinks about a job hunt: what showed up today?
        section.property: "postedDayText"
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section

            height: 34
            verticalAlignment: Text.AlignVCenter
            text: section
            color: JobCrushTheme.secondaryTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }

        delegate: Rectangle {
            id: discoveredJobRow

            required property int index
            required property string positionTitle
            required property string companyName
            required property string locationText
            required property string salaryText
            required property string summaryLine
            required property string sourceDisplayName
            required property int matchScore
            required property string matchReasonsText
            required property bool isRemoteRole

            width: ListView.view.width
            height: discoveredJobRowColumn.implicitHeight + 24
            radius: 8
            color: discoveredJobRowMouseArea.containsMouse
                ? JobCrushTheme.cardBackgroundColor : JobCrushTheme.panelBackgroundColor
            border.color: JobCrushTheme.hairlineBorderColor
            border.width: 1

            // The match badge. Only shown once there is a profile to match
            // against — a score of 0 for everything would be noise dressed up
            // as information.
            Rectangle {
                id: matchScoreBadge
                visible: discoveriesPage.discoveredJobListViewModel.canRankProspects
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 14
                width: 46
                height: 26
                radius: 6
                color: "transparent"
                border.width: discoveredJobRow.matchScore >= 60 ? 2 : 1
                border.color: discoveredJobRow.matchScore >= 60
                    ? JobCrushTheme.positiveColor
                    : (discoveredJobRow.matchScore >= 30
                           ? JobCrushTheme.accentColor
                           : JobCrushTheme.hairlineBorderColor)

                Text {
                    anchors.centerIn: parent
                    text: discoveredJobRow.matchScore
                    color: discoveredJobRow.matchScore >= 60
                        ? JobCrushTheme.positiveColor
                        : (discoveredJobRow.matchScore >= 30
                               ? JobCrushTheme.accentColor
                               : JobCrushTheme.mutedTextColor)
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: Font.Bold
                }
            }

            Column {
                id: discoveredJobRowColumn
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: matchScoreBadge.visible ? matchScoreBadge.left : parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 12
                spacing: 4

                Text {
                    width: parent.width
                    text: discoveredJobRow.positionTitle
                    color: JobCrushTheme.primaryTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: discoveredJobRow.companyName
                          + (discoveredJobRow.locationText.length > 0
                                 ? "   ·   " + discoveredJobRow.locationText : "")
                          + (discoveredJobRow.isRemoteRole ? "   ·   Remote" : "")
                          + (discoveredJobRow.salaryText.length > 0
                                 ? "   ·   " + discoveredJobRow.salaryText : "")
                          + "   ·   via " + discoveredJobRow.sourceDisplayName
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }

                // Why it ranked where it did. A ranking nobody can question is
                // a ranking nobody can trust.
                Text {
                    width: parent.width
                    visible: discoveredJobRow.matchReasonsText.length > 0
                             && discoveriesPage.discoveredJobListViewModel.canRankProspects
                    text: discoveredJobRow.matchReasonsText
                    color: JobCrushTheme.positiveColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    visible: discoveredJobRow.summaryLine.length > 0
                    text: discoveredJobRow.summaryLine
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: discoveredJobRowMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // Opens the posting where it was published. Job Crush links
                // out; it never republishes somebody else's listing as its own.
                onClicked: discoveriesPage.discoveredJobListViewModel
                    .openJobPostingInBrowser(discoveredJobRow.index)
            }
        }
    }

    // Always on screen while there is anything below the fold — both so it is
    // findable and so its length says how much more there is.
    VerticalScrollBar {
        anchors.top: discoveredJobListView.top
        anchors.bottom: discoveredJobListView.bottom
        anchors.right: parent.right
        anchors.rightMargin: 18
        flickableTarget: discoveredJobListView
    }

    // ------------------------------------------------------------------
    // Empty states — each says what is actually true and what to do next
    // ------------------------------------------------------------------
    Rectangle {
        id: discoveriesEmptyStateCard

        anchors.centerIn: discoveredJobListView
        visible: discoveredJobListView.count === 0
        width: 420
        height: emptyStateColumn.implicitHeight + 48
        radius: 12
        color: JobCrushTheme.panelBackgroundColor
        border.color: JobCrushTheme.hairlineBorderColor
        border.width: 1

        readonly property bool noSitesTicked:
            discoveriesPage.jobSourceRosterViewModel.enabledJobSourceTabs.length === 0
        readonly property bool noProfileYet:
            !discoveriesPage.discoveredJobListViewModel.canRankProspects

        Column {
            id: emptyStateColumn
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 12

            Text {
                width: parent.width
                text: discoveriesEmptyStateCard.noSitesTicked
                    ? "No job sites are switched on"
                    : (discoveriesEmptyStateCard.noProfileYet
                           ? "Tell Job Crush what you're after"
                           : "Nothing found yet")
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize + 2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: discoveriesEmptyStateCard.noSitesTicked
                    ? "Pick the job sites you want Job Crush to watch in Settings. "
                      + "Every one of them is free."
                    : (discoveriesEmptyStateCard.noProfileYet
                           ? "Add the job titles you're going for and the skills you "
                             + "bring, over in Settings. Top Prospects ranks against "
                             + "those — no AI usage, no waiting."
                           : "Hit Scout now and Job Crush will go look.")
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: discoveriesEmptyStateCard.noSitesTicked || discoveriesEmptyStateCard.noProfileYet
                width: openSettingsFromDiscoveriesLabel.implicitWidth + 36
                height: 36
                radius: 8
                color: openSettingsFromDiscoveriesMouseArea.containsMouse
                    ? Qt.lighter(JobCrushTheme.callToActionColor, 1.15)
                    : JobCrushTheme.callToActionColor

                Text {
                    id: openSettingsFromDiscoveriesLabel
                    anchors.centerIn: parent
                    text: "Open Settings"
                    color: JobCrushTheme.onAccentTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: openSettingsFromDiscoveriesMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: discoveriesPage.settingsRequested()
                }
            }
        }
    }
}
