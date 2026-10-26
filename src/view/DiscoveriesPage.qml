import QtQuick
import JobCrush

// DiscoveriesPage
//
// This page lists the jobs that JobScout found. JobScout is the C++ class
// that searches the job sites. One search of all the sites the user turned
// on is called a sweep. The Scout now button starts a sweep.
//
// The page has these tabs, in this order:
//  - Top Prospects: every job, ranked best match first.
//  - Manual Add: the jobs the user added by hand.
//  - One tab for each job site the user turned on in Settings. If the user
//    turns a site off in Settings, its tab goes away here.
//  - Outside your area: jobs outside the places the user said they would
//    work. The location filter hides these jobs from the other tabs.
//
// The CRUSH button puts a job on the user's board (the Job Pipelines
// page). Nothing else on this page does that.
//
// This file is the view layer. It shows data from its viewmodels and asks
// Main to change pages. It has no business logic.
Rectangle {
    id: discoveriesPage

    // Set by Main.
    property DiscoveredJobListViewModel discoveredJobListViewModel
    property JobSourceRosterViewModel jobSourceRosterViewModel

    // Asks Main to show the Settings page. The Open Settings button on the
    // empty-list card sends this signal.
    signal settingsRequested

    color: JobCrushTheme.appBackgroundColor

    // The name of the tab that is showing. The viewmodel stores this value.
    // The page reads it here, and changes it only through selectTab().
    readonly property string activeTabSourceName:
        discoveredJobListViewModel.activeTabSourceName

    readonly property bool showingTopProspects: activeTabSourceName === ""
    readonly property bool showingManualAdd: discoveredJobListViewModel.showingManualAdd
    readonly property bool showingOutsideSearchArea:
        discoveredJobListViewModel.showingOutsideSearchArea

    readonly property string manualAddTabSourceName:
        discoveredJobListViewModel.manualAddTabSourceName
    readonly property string outsideSearchAreaTabSourceName:
        discoveredJobListViewModel.outsideSearchAreaTabSourceName

    // Shows the tab named tabSourceName. An empty string means Top Prospects.
    function selectTab(tabSourceName) {
        discoveredJobListViewModel.activeTabSourceName = tabSourceName
    }

    // After the user adds a job by hand and presses "Show me" in the add
    // form, the page draws an outline around that job. The outline stays
    // until highlightFadeTimer's interval runs out. The page finds that job
    // by its row number in the list. Each tab has its own list. Row 7 on one
    // tab is a different job than row 7 on another tab. So when the user
    // switches tabs, this code removes the outline and stops its timer.
    // Otherwise the outline would land on the wrong job.
    onActiveTabSourceNameChanged: {
        discoveriesPage.highlightedRowIndex = -1
        highlightFadeTimer.stop()
    }

    // The message about what the last CRUSH press did. A strip at the bottom
    // of the page shows it. An empty string hides the strip. The page uses a
    // strip and not a dialog, because the user may press CRUSH many times in
    // a row. A dialog for each press would get in the way.
    property string crushOutcomeText: ""

    // The row number of the job to outline, or -1 for no outline. The page
    // sets this after it scrolls to the job the user just added.
    // highlightFadeTimer sets it back to -1 when its interval runs out. By
    // then the user has seen where the job is.
    property int highlightedRowIndex: -1

    Timer {
        id: highlightFadeTimer
        interval: 6000
        onTriggered: discoveriesPage.highlightedRowIndex = -1
    }

    // The two functions below take the user to the job they just added by
    // hand. They switch to the Manual Add tab, find the job's row, scroll to
    // it and outline it.
    //
    // The viewmodel builds the rows on a background thread. So right after
    // the tab switch, the job may not be in the rows yet. If it is not, this
    // flag is set to true. When the next rows finish loading, the
    // Connections block below tries once more and clears the flag.
    property bool jumpToTheJobJustAddedIsWaiting: false

    function showTheJobJustAdded() {
        discoveriesPage.selectTab(discoveriesPage.manualAddTabSourceName)
        if (!discoveriesPage.jumpToTheJobJustAddedIfItIsThere()) {
            discoveriesPage.jumpToTheJobJustAddedIsWaiting = true
        }
    }

    function jumpToTheJobJustAddedIfItIsThere() {
        const rowIndex = discoveredJobListViewModel.rowOfMostRecentlyHandAddedJob()
        if (rowIndex < 0) {
            return false
        }
        discoveredJobListView.positionViewAtIndex(rowIndex, ListView.Beginning)
        discoveriesPage.highlightedRowIndex = rowIndex
        highlightFadeTimer.restart()
        return true
    }

    Connections {
        target: discoveriesPage.discoveredJobListViewModel
        function onDiscoveredJobsChanged() {
            if (discoveriesPage.jumpToTheJobJustAddedIsWaiting
                    && !discoveriesPage.discoveredJobListViewModel.rowsAreLoading) {
                discoveriesPage.jumpToTheJobJustAddedIsWaiting = false
                discoveriesPage.jumpToTheJobJustAddedIfItIsThere()
            }
        }
    }

    // Whether the form for adding one job by hand is open. The form opens
    // when the user goes to the Manual Add tab, and it closes when the user
    // goes to any other tab. The Close button hides it while the user stays
    // on Manual Add. On other tabs the form would only push the rows down.
    property bool addOneJobPanelIsOpen: false

    // True when the Manual Add tab is the one showing.
    readonly property bool manualAddTabIsShowing:
        activeTabSourceName === manualAddTabSourceName

    onManualAddTabIsShowingChanged: addOneJobPanelIsOpen = manualAddTabIsShowing
    Component.onCompleted: addOneJobPanelIsOpen = manualAddTabIsShowing

    // If the user turns off the job site whose tab is showing, this code
    // switches to Top Prospects. Otherwise the page would stay on a tab that
    // no longer exists.
    Connections {
        target: discoveriesPage.jobSourceRosterViewModel
        function onEnabledSourcesChanged() {
            // Only a site's tab can go away. Top Prospects, Manual Add and
            // Outside your area are not sites.
            if (discoveriesPage.showingTopProspects
                    || discoveriesPage.showingManualAdd
                    || discoveriesPage.showingOutsideSearchArea) {
                return
            }
            const stillEnabled = discoveriesPage.jobSourceRosterViewModel
                .enabledJobSourceTabs.some(function(jobSourceTab) {
                    return jobSourceTab.storageName === discoveriesPage.activeTabSourceName
                })
            if (!stillEnabled) {
                discoveriesPage.selectTab("")
            }
        }
    }

    // One tab button in the row of tabs. Every tab uses this button. A tab
    // can change its colors through the color properties below. The Outside
    // your area tab does this.
    component DiscoveryTabButton: Rectangle {
        id: discoveryTabButton

        property string labelText
        property bool isActiveTab: false
        // Every selected tab has the accent color (cyan in the Classic
        // theme), the same color as a selected item on other pages. Pink is
        // used only for buttons that start an action, like CRUSH and
        // Scout now.
        property color activeBorderColor: JobCrushTheme.accentColor
        property color idleBorderColor: JobCrushTheme.hairlineBorderColor
        property color idleLabelColor: JobCrushTheme.secondaryTextColor

        signal tabClicked

        width: visible ? tabLabel.implicitWidth + 28 : 0
        height: 32
        radius: 16
        color: isActiveTab ? JobCrushTheme.cardBackgroundColor : "transparent"
        border.color: isActiveTab ? activeBorderColor : idleBorderColor
        // The active tab gets a thicker border, so the user can pick it out
        // without color. This matters in the Grayscale theme, which has no
        // accent color.
        border.width: isActiveTab ? 2 : 1

        GlowHalo {
            glowColor: discoveryTabButton.border.color
            cornerRadius: discoveryTabButton.radius
            isLit: discoveryTabButton.isActiveTab
        }

        Text {
            id: tabLabel
            anchors.centerIn: parent
            text: discoveryTabButton.labelText
            textFormat: Text.PlainText
            color: discoveryTabButton.isActiveTab
                ? JobCrushTheme.primaryTextColor : discoveryTabButton.idleLabelColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: discoveryTabButton.isActiveTab ? Font.DemiBold : Font.Normal
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: discoveryTabButton.tabClicked()
        }
    }

    // ------------------------------------------------------------------
    // Header: the title, the sweep's progress, and the two buttons
    // ------------------------------------------------------------------
    Item {
        id: discoveriesHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            id: discoveriesTitle
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "Discoveries"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        // This label shows how the sweep is going, with real job counts from
        // each site. The page shows this label instead of a spinner.
        Text {
            id: sweepProgressLabel
            anchors.verticalCenter: parent.verticalCenter
            // The label's left edge is anchored to the title's right edge,
            // not set at a fixed position. So if the font or the title text
            // changes, the label moves with the title and never covers it.
            anchors.left: discoveriesTitle.right
            anchors.leftMargin: 20
            anchors.right: addOneJobButton.left
            anchors.rightMargin: 20
            // While a sweep runs, the label shows the running counts. After
            // the sweep, the label shows the trouble and notice lines from
            // the last sweep. If there are none, it shows the summary. The
            // trouble lines take the place of the summary, because a site
            // that failed is what the user most needs to know about.
            text: {
                const listViewModel = discoveriesPage.discoveredJobListViewModel
                if (listViewModel.sweepIsRunning) {
                    return listViewModel.sweepProgressText
                }
                const troubleAndNotices = [listViewModel.lastSweepTroubleText,
                                           listViewModel.lastSweepNoticeText]
                    .filter(function(line) { return line.length > 0 })
                    .join("   ·   ")
                if (troubleAndNotices.length > 0) {
                    return troubleAndNotices
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

            // Two lines, because a sweep across several sites has more to say
            // than fits on one line.
            wrapMode: Text.WordWrap
            maximumLineCount: 2
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

        // The button for adding one job by hand. It is placed next to the
        // Scout now button, because these two buttons are the two ways jobs
        // get onto this page.
        //
        // When the form is closed, the button says "Add a job". Pressing it
        // goes to the Manual Add tab, and the form opens there. When the form
        // is open, the button says "Close", and pressing it hides the form.
        Rectangle {
            id: addOneJobButton

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: scoutNowButton.left
            anchors.rightMargin: 10
            width: addOneJobButtonLabel.implicitWidth + 26
            height: 36
            radius: 8
            color: discoveriesPage.addOneJobPanelIsOpen
                ? JobCrushTheme.cardBackgroundColor
                : (addOneJobMouseArea.containsMouse
                       ? JobCrushTheme.cardBackgroundColor : "transparent")
            border.width: discoveriesPage.addOneJobPanelIsOpen ? 2 : 1
            border.color: discoveriesPage.addOneJobPanelIsOpen
                ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor

            Text {
                id: addOneJobButtonLabel
                anchors.centerIn: parent
                text: discoveriesPage.addOneJobPanelIsOpen ? "Close" : "Add a job"
                color: discoveriesPage.addOneJobPanelIsOpen
                    ? JobCrushTheme.primaryTextColor : JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: addOneJobMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (discoveriesPage.addOneJobPanelIsOpen) {
                        discoveriesPage.addOneJobPanelIsOpen = false
                    } else if (!discoveriesPage.manualAddTabIsShowing) {
                        // Go to the Manual Add tab. The form opens by
                        // itself, because the page opens the form whenever
                        // the Manual Add tab starts to show.
                        discoveriesPage.selectTab(discoveriesPage.manualAddTabSourceName)
                    } else {
                        discoveriesPage.addOneJobPanelIsOpen = true
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
                    ? "Scouting…" : "Scout now"
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
    // Tabs: Top Prospects, Manual Add, one per site, then Outside your area
    // ------------------------------------------------------------------
    Row {
        id: discoveryTabRow
        anchors.top: discoveriesHeader.bottom
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.right: parent.right
        anchors.topMargin: 14
        spacing: 10

        // Top Prospects comes first. Its list is ranked best match first, so
        // it answers "what should I look at first?"
        DiscoveryTabButton {
            labelText: "Top Prospects"
            isActiveTab: discoveriesPage.showingTopProspects
            onTabClicked: discoveriesPage.selectTab("")
        }

        // Manual Add comes second, ahead of every site. These are jobs the
        // user found and added by hand, so they must always be easy to find.
        DiscoveryTabButton {
            labelText: "Manual Add"
            isActiveTab: discoveriesPage.showingManualAdd
            onTabClicked: discoveriesPage.selectTab(discoveriesPage.manualAddTabSourceName)
        }

        Repeater {
            model: discoveriesPage.jobSourceRosterViewModel.enabledJobSourceTabs

            delegate: DiscoveryTabButton {
                id: jobSourceTab

                required property var modelData

                labelText: modelData.displayName
                isActiveTab: discoveriesPage.activeTabSourceName === modelData.storageName
                onTabClicked: discoveriesPage.selectTab(jobSourceTab.modelData.storageName)
            }
        }

        // The Outside your area tab. It lists jobs from every site that are
        // outside the places the user said they would work. The location
        // filter hides these jobs from the other tabs. The tab is gold, not
        // the usual color, because the user does not see these jobs anywhere
        // else. It is last in the row because it looks different from the
        // other tabs.
        //
        // The tab shows only when the user has named at least one place. If
        // no place is named, the filter hides no jobs, and the tab would
        // always be empty.
        DiscoveryTabButton {
            visible: discoveriesPage.discoveredJobListViewModel.searchAreaIsNarrowed
            labelText: "Outside your area ("
                       + discoveriesPage.discoveredJobListViewModel.jobCountOutsideSearchArea
                       + ")"
            isActiveTab: discoveriesPage.showingOutsideSearchArea
            idleBorderColor: JobCrushTheme.noticeTextColor
            idleLabelColor: JobCrushTheme.noticeTextColor
            onTabClicked: discoveriesPage.selectTab(
                discoveriesPage.outsideSearchAreaTabSourceName)
        }
    }

    // The panel for adding one job by hand. It opens below the tabs.
    AddOneJobPanel {
        id: addOneJobPanel

        anchors.top: discoveryTabRow.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.right: parent.right
        anchors.rightMargin: 46

        visible: discoveriesPage.addOneJobPanelIsOpen
        discoveredJobListViewModel: discoveriesPage.discoveredJobListViewModel
        onShowTheJobJustAddedRequested: discoveriesPage.showTheJobJustAdded()
    }

    // A bar that says how many jobs the location filter is hiding. Its
    // "Show me" button opens the Outside your area tab. On that tab, the bar
    // says so instead, and its button goes back to Top Prospects.
    //
    // It is a full bar, not a line of small print, because the app is hiding
    // jobs and the user must not miss that. If the user missed it, the page
    // would look like the sweep found nothing.
    Rectangle {
        id: outsideSearchAreaBar

        anchors.top: addOneJobPanel.visible
            ? addOneJobPanel.bottom : discoveryTabRow.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.right: parent.right
        anchors.rightMargin: 46

        readonly property int heldBackCount:
            discoveriesPage.discoveredJobListViewModel.jobCountOutsideSearchArea
        readonly property bool showingOutside: discoveriesPage.showingOutsideSearchArea

        // The bar never shows on the Manual Add tab. The Manual Add list does
        // not use the location filter, so a bar that says jobs are hidden
        // would be wrong there.
        visible: discoveriesPage.discoveredJobListViewModel.searchAreaIsNarrowed
                 && !discoveriesPage.showingManualAdd
                 && (heldBackCount > 0 || showingOutside)

        height: visible ? 46 : 0
        radius: 8
        color: showingOutside
            ? JobCrushTheme.cardBackgroundColor : JobCrushTheme.panelBackgroundColor
        border.width: 2
        border.color: showingOutside
            ? JobCrushTheme.callToActionColor : JobCrushTheme.noticeTextColor

        // A stripe of color down the left edge, so the bar is noticed before
        // it is read.
        Rectangle {
            width: 5
            height: parent.height - 16
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            radius: 2
            color: outsideSearchAreaBar.showingOutside
                ? JobCrushTheme.callToActionColor : JobCrushTheme.noticeTextColor
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 26
            anchors.right: outsideSearchAreaButton.left
            anchors.rightMargin: 16
            text: outsideSearchAreaBar.showingOutside
                ? "These jobs are OUTSIDE where you said you'd work"
                : outsideSearchAreaBar.heldBackCount
                  + (outsideSearchAreaBar.heldBackCount === 1
                         ? " job is hidden" : " jobs are hidden")
                  + " — they're outside where you said you'd work"
            color: outsideSearchAreaBar.showingOutside
                ? JobCrushTheme.callToActionColor : JobCrushTheme.noticeTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Rectangle {
            id: outsideSearchAreaButton
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 10
            width: outsideSearchAreaButtonLabel.implicitWidth + 28
            height: 30
            radius: 8
            color: outsideSearchAreaButtonMouseArea.containsMouse
                ? Qt.lighter(outsideSearchAreaBar.showingOutside
                                 ? JobCrushTheme.callToActionColor
                                 : JobCrushTheme.noticeTextColor, 1.12)
                : (outsideSearchAreaBar.showingOutside
                       ? JobCrushTheme.callToActionColor : JobCrushTheme.noticeTextColor)

            Text {
                id: outsideSearchAreaButtonLabel
                anchors.centerIn: parent
                text: outsideSearchAreaBar.showingOutside
                    ? "Back to my area" : "Show me"
                color: JobCrushTheme.onAccentTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: outsideSearchAreaButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // "Show me" goes to the Outside your area tab. "Back to my
                // area" goes to Top Prospects.
                onClicked: discoveriesPage.selectTab(
                    outsideSearchAreaBar.showingOutside
                        ? "" : discoveriesPage.outsideSearchAreaTabSourceName)
            }
        }
    }

    // ------------------------------------------------------------------
    // The list of jobs, one row per job
    // ------------------------------------------------------------------
    ListView {
        id: discoveredJobListView

        anchors.top: outsideSearchAreaBar.visible
            ? outsideSearchAreaBar.bottom
            : (addOneJobPanel.visible ? addOneJobPanel.bottom : discoveryTabRow.bottom)
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

        // One notch of the mouse wheel scrolls twice as far as Qt's default,
        // which felt slow on a long list of job rows.
        WheelHandler {
            id: discoveryWheelHandler

            // Qt reports one wheel notch as 120 units of angleDelta.
            readonly property real angleDeltaPerNotch: 120
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
                    - (wheelEvent.angleDelta.y / angleDeltaPerNotch)
                      * pixelsPerNotch * speedMultiplier
                discoveredJobListView.contentY = Math.max(
                    discoveredJobListView.originY,
                    Math.min(discoveredJobListView.originY + scrollableDistance,
                             requestedContentY))
            }
        }

        // Rows are grouped under the day the job was posted, so the user can
        // see what is new today.
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
            required property string matchStrength
            required property bool isRemoteRole
            required property string postingSourceDisplayName
            required property bool wasAddedByHand
            required property bool isManualAllTheWay
            required property bool postingSeemsGone
            required property string postingGoneText
            required property bool asksForClearance
            required property bool isOnTheBoard

            readonly property bool isTheJobJustAdded:
                discoveriesPage.highlightedRowIndex === discoveredJobRow.index

            width: ListView.view.width

            // The row's height comes from text a job board wrote. The text
            // is cleaned before it gets here, but if some markup gets past
            // the cleaning, this cap stops it from making a row a screen tall.
            readonly property int tallestRowHeight: 162
            height: Math.min(tallestRowHeight,
                             Math.max(rowRightLane.implicitHeight + 24,
                                      discoveredJobRowColumn.implicitHeight + 24))
            clip: true
            radius: 8
            color: discoveredJobRowMouseArea.containsMouse
                ? JobCrushTheme.cardBackgroundColor : JobCrushTheme.panelBackgroundColor
            // The row's outline shows one of four cases. If more than one
            // case is true, the first one in this list decides the color:
            //  1. This is the job the user just added and asked to see. The
            //     outline helps them find it at a glance.
            //  2. The user added this job and no job board had it. The
            //     posting is incomplete, and the user should fill in the rest.
            //  3. The company's own job board no longer lists this job.
            //  4. None of the above. The row gets the normal thin line.
            // Cases 1 to 3 use a 2-pixel outline. Case 4 uses 1 pixel.
            border.color: discoveredJobRow.isTheJobJustAdded
                ? JobCrushTheme.callToActionColor
                : (discoveredJobRow.isManualAllTheWay
                       ? JobCrushTheme.pendingColor
                       : (discoveredJobRow.postingSeemsGone
                              ? JobCrushTheme.noticeTextColor
                              : JobCrushTheme.hairlineBorderColor))
            border.width: (discoveredJobRow.isTheJobJustAdded
                           || discoveredJobRow.isManualAllTheWay
                           || discoveredJobRow.postingSeemsGone) ? 2 : 1

            // An empty item that marks the right-hand column of the row. The
            // match score badge and the CRUSH button under it are anchored to
            // this column. The text column on the left ends at this column's
            // left edge. Before this column existed, the summary line ran
            // under the CRUSH button.
            Item {
                id: rowRightLane
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 12
                width: 84
                implicitHeight: matchScoreBadge.height + 8 + crushButton.height
            }

            // The match score badge. The score says how well this job fits
            // the user's search profile, from 0 to 100, so it is shown as a
            // percent, like "46%". The percent sign tells the user the number
            // is a score and not a count or a job number.
            //
            // The badge shows only when the user has a search profile. Without
            // a profile there is nothing to score against, and every job would
            // show 0%.
            Rectangle {
                id: matchScoreBadge
                visible: discoveriesPage.discoveredJobListViewModel.canRankProspects
                anchors.right: rowRightLane.right
                anchors.top: rowRightLane.top
                width: Math.max(46, matchScoreText.implicitWidth + 16)
                height: 26
                radius: 6
                color: "transparent"
                border.width: discoveredJobRow.matchStrength === "strong" ? 2 : 1
                border.color: discoveredJobRow.matchStrength === "strong"
                    ? JobCrushTheme.positiveColor
                    : (discoveredJobRow.matchStrength === "fair"
                           ? JobCrushTheme.accentColor
                           : JobCrushTheme.hairlineBorderColor)

                Text {
                    id: matchScoreText
                    anchors.centerIn: parent
                    text: discoveredJobRow.matchScore + "%"
                    color: discoveredJobRow.matchStrength === "strong"
                        ? JobCrushTheme.positiveColor
                        : (discoveredJobRow.matchStrength === "fair"
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
                anchors.right: rowRightLane.left
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 12
                spacing: 4

                // Every Text below that shows words from a job board sets
                // textFormat to Text.PlainText.
                //
                // Qt's default is AutoText, which shows a string as rich text
                // when it looks like markup. That would let a job board decide
                // what is drawn in this window, so it is always plain text
                // here. This is intentional for the sake of security.
                Text {
                    width: parent.width
                    text: discoveredJobRow.positionTitle
                    textFormat: Text.PlainText
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
                          // "Remote" is added only when the location does not
                          // already say it, so the row never says it twice.
                          + (discoveredJobRow.isRemoteRole
                             && discoveredJobRow.locationText.toLowerCase().indexOf("remote") < 0
                                 ? "   ·   Remote" : "")
                          + (discoveredJobRow.salaryText.length > 0
                                 ? "   ·   " + discoveredJobRow.salaryText : "")
                          + (discoveredJobRow.postingSourceDisplayName.length > 0
                                 ? "   ·   via " + discoveredJobRow.postingSourceDisplayName
                                 : "")
                    textFormat: Text.PlainText
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }

                // This line shows only for a job the user added by hand. It
                // says which of two things happened: no job board had the
                // job, or Job Crush found the posting on a named site.
                Text {
                    width: parent.width
                    visible: discoveredJobRow.wasAddedByHand
                    text: discoveredJobRow.isManualAllTheWay
                        ? "You added this. No board had it — fill in the rest yourself."
                        : "You added this. Job Crush found the posting on "
                          + discoveredJobRow.postingSourceDisplayName + "."
                    textFormat: Text.PlainText
                    color: discoveredJobRow.isManualAllTheWay
                        ? JobCrushTheme.pendingColor : JobCrushTheme.positiveColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                // Defense jobs often need a security clearance. The row says
                // so, so the user does not have to read the whole posting to
                // find out.
                Text {
                    width: parent.width
                    visible: discoveredJobRow.asksForClearance
                    text: "Asks for a security clearance"
                    textFormat: Text.PlainText
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.italic: true
                    elide: Text.ElideRight
                }

                // This line shows when the company's own job board no longer
                // lists this job. It uses the same amber color as this job's
                // card on the user's board (the Job Pipelines page).
                Text {
                    width: parent.width
                    visible: discoveredJobRow.postingSeemsGone
                    text: discoveredJobRow.postingGoneText
                    textFormat: Text.PlainText
                    color: JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                // Why the job got its score, so the user can judge the
                // ranking for themselves.
                Text {
                    width: parent.width
                    visible: discoveredJobRow.matchReasonsText.length > 0
                             && discoveriesPage.discoveredJobListViewModel.canRankProspects
                    text: discoveredJobRow.matchReasonsText
                    textFormat: Text.PlainText
                    color: JobCrushTheme.positiveColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    visible: discoveredJobRow.summaryLine.length > 0
                    text: discoveredJobRow.summaryLine
                    textFormat: Text.PlainText
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }
            }

            // CRUSH puts the job on the user's board. The button is on the
            // row, so the user can act while looking at the job.
            Rectangle {
                id: crushButton

                anchors.right: rowRightLane.right
                anchors.top: matchScoreBadge.visible
                    ? matchScoreBadge.bottom : rowRightLane.top
                anchors.topMargin: matchScoreBadge.visible ? 8 : 0
                width: Math.max(crushButtonLabel.implicitWidth + 24, 78)
                height: 28
                radius: 6
                z: 2

                readonly property bool alreadyOnTheBoard: discoveredJobRow.isOnTheBoard

                color: crushButton.alreadyOnTheBoard
                    ? "transparent"
                    : (crushButtonMouseArea.containsMouse
                           ? Qt.lighter(JobCrushTheme.callToActionColor, 1.15)
                           : JobCrushTheme.callToActionColor)
                border.width: crushButton.alreadyOnTheBoard ? 1 : 0
                border.color: JobCrushTheme.positiveColor

                Text {
                    id: crushButtonLabel
                    anchors.centerIn: parent
                    // Once the job is on the board, the button says so and
                    // can no longer be pressed.
                    text: crushButton.alreadyOnTheBoard ? "on your board" : "CRUSH"
                    color: crushButton.alreadyOnTheBoard
                        ? JobCrushTheme.positiveColor : JobCrushTheme.onAccentTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: Font.Bold
                    font.letterSpacing: crushButton.alreadyOnTheBoard ? 0 : 1
                }

                MouseArea {
                    id: crushButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !crushButton.alreadyOnTheBoard
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: discoveriesPage.crushOutcomeText
                        = discoveriesPage.discoveredJobListViewModel
                              .crushJobPostingAt(discoveredJobRow.index)
                }
            }

            MouseArea {
                id: discoveredJobRowMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // Clicking the row opens the posting in the web browser, on
                // the site that published it. Job Crush links to the posting
                // and never shows the posting's content as its own.
                onClicked: discoveriesPage.discoveredJobListViewModel
                    .openJobPostingInBrowser(discoveredJobRow.index)
            }
        }
    }

    // The scroll bar shows whenever the list is too long to fit, so the user
    // can see how much more there is.
    VerticalScrollBar {
        anchors.top: discoveredJobListView.top
        anchors.bottom: discoveredJobListView.bottom
        anchors.right: parent.right
        anchors.rightMargin: 18
        flickableTarget: discoveredJobListView
    }

    // ------------------------------------------------------------------
    // What the last CRUSH did
    // ------------------------------------------------------------------
    Rectangle {
        id: crushOutcomeStrip

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        visible: discoveriesPage.crushOutcomeText.length > 0
        height: 42
        radius: 8
        z: 20
        color: JobCrushTheme.panelBackgroundColor
        border.width: 2
        border.color: JobCrushTheme.positiveColor

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: dismissCrushOutcome.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: discoveriesPage.crushOutcomeText
            textFormat: Text.PlainText
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            elide: Text.ElideRight
        }

        Text {
            id: dismissCrushOutcome
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "×"
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize

            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                cursorShape: Qt.PointingHandCursor
                onClicked: discoveriesPage.crushOutcomeText = ""
            }
        }
    }

    // ------------------------------------------------------------------
    // Empty states. Each one says why the list is empty and what to do
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

        // True while the viewmodel builds the rows on a background thread.
        // While it is true, the card says "Getting these jobs ready…" and not
        // "Nothing found yet".
        readonly property bool rowsAreLoading:
            discoveriesPage.discoveredJobListViewModel.rowsAreLoading
        readonly property bool noSitesTicked:
            discoveriesPage.jobSourceRosterViewModel.enabledJobSourceTabs.length === 0
        readonly property bool noProfileYet:
            !discoveriesPage.discoveredJobListViewModel.canRankProspects
        // True when this list is empty but the location filter is hiding at
        // least one job. Then "Nothing found yet" would be wrong, because
        // jobs were found.
        readonly property bool everythingFilteredOut:
            !noSitesTicked
            && discoveriesPage.discoveredJobListViewModel.searchAreaIsNarrowed
            && discoveriesPage.discoveredJobListViewModel.jobCountOutsideSearchArea > 0
            && !discoveriesPage.showingOutsideSearchArea

        Column {
            id: emptyStateColumn
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 12

            Text {
                width: parent.width
                text: discoveriesEmptyStateCard.rowsAreLoading
                    ? "Getting these jobs ready…"
                    : discoveriesEmptyStateCard.noSitesTicked
                    ? "No job sites are switched on"
                    : (discoveriesEmptyStateCard.everythingFilteredOut
                           ? "Nothing where you said you'd work"
                           : (discoveriesEmptyStateCard.noProfileYet
                                  ? "Tell Job Crush what you're after"
                                  : "Nothing found yet"))
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize + 2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                visible: !discoveriesEmptyStateCard.rowsAreLoading
                text: discoveriesEmptyStateCard.noSitesTicked
                    ? "Pick the job sites you want Job Crush to watch in Settings. "
                      + "Every one of them is free."
                    : (discoveriesEmptyStateCard.everythingFilteredOut
                           ? "Job Crush did find jobs — every one of them is somewhere "
                             + "other than the places you listed. Widen the list in "
                             + "Settings, or use the link above to look at them anyway."
                           : (discoveriesEmptyStateCard.noProfileYet
                                  ? "Add the job titles you're going for and the skills "
                                    + "you bring, over in Settings. Top Prospects ranks "
                                    + "against those — no AI usage, no waiting."
                                  : "Hit Scout now and Job Crush will go look."))
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !discoveriesEmptyStateCard.rowsAreLoading
                         && (discoveriesEmptyStateCard.noSitesTicked
                             || discoveriesEmptyStateCard.noProfileYet
                             || discoveriesEmptyStateCard.everythingFilteredOut)
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
