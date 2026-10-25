import QtQuick
import JobCrush

// StatsPage
//
// The numbers on a job search.
//
// The charts are plain rectangles in a Repeater. No chart library. Qt Charts
// is GPL only and this project is MIT. A bar whose height is a binding is also
// easy to theme and change.
//
// The rule for this page: a job search produces small, discouraging numbers.
// Report them honestly and do not scold. Only show a percent when there is
// enough data for one. Count reading and drafting as work, because they are.
//
// This is the view layer. The counting is done in JobSearchStatistics in
// the logic layer. This page only draws the numbers it is given.
Rectangle {
    id: statsPage

    // Injected from Main.
    property JobSearchStatsViewModel jobSearchStatsViewModel

    color: JobCrushTheme.appBackgroundColor

    Item {
        id: statsHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            id: statsTitle
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "Stats"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: statsTitle.right
            anchors.leftMargin: 14
            text: "The last six months of it"
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: JobCrushTheme.hairlineBorderColor
        }
    }

    // Nothing counted yet.
    Text {
        anchors.centerIn: parent
        width: 460
        visible: !statsPage.jobSearchStatsViewModel.hasAnythingToShow
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: statsPage.jobSearchStatsViewModel.emptyStateText
        color: JobCrushTheme.mutedTextColor
        font.pixelSize: JobCrushTheme.bodyFontSize
    }

    Flickable {
        id: statsScroller
        anchors.top: statsHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 28
        anchors.rightMargin: 44
        visible: statsPage.jobSearchStatsViewModel.hasAnythingToShow
        clip: true
        contentWidth: width
        contentHeight: statsColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: statsColumn
            width: statsScroller.width
            spacing: 22


            // ----------------------------------------------------------
            // The four headline numbers.
            // ----------------------------------------------------------
            Row {
                id: headlineTileRow

                readonly property int headlineTileCount: 4

                width: parent.width
                spacing: 14

                Repeater {
                    model: [
                        {
                            headline: statsPage.jobSearchStatsViewModel.onTheBoardText,
                            label: "on the board",
                            explanation: "jobs you decided were worth it"
                        },
                        {
                            headline: statsPage.jobSearchStatsViewModel.appliedText,
                            label: "sent",
                            explanation: "applications out the door"
                        },
                        {
                            headline: statsPage.jobSearchStatsViewModel.replyRateText,
                            label: "went somewhere",
                            explanation: statsPage.jobSearchStatsViewModel.replyRateExplanationText
                        },
                        {
                            headline: statsPage.jobSearchStatsViewModel.streakText,
                            label: "on the trot",
                            explanation: statsPage.jobSearchStatsViewModel.streakExplanationText
                        }
                    ]

                    delegate: Rectangle {
                        id: statTile

                        required property var modelData

                        // The tiles share the width evenly, after the gaps
                        // between them.
                        width: (statsColumn.width
                                - (headlineTileRow.headlineTileCount - 1) * headlineTileRow.spacing)
                               / headlineTileRow.headlineTileCount
                        height: statTileColumn.implicitHeight + 32
                        radius: 8
                        color: JobCrushTheme.panelBackgroundColor
                        border.width: 1
                        border.color: JobCrushTheme.hairlineBorderColor

                        Column {
                            id: statTileColumn
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 16
                            spacing: 4

                            Text {
                                text: statTile.modelData.headline
                                color: JobCrushTheme.accentColor
                                font.pixelSize: 30
                                font.weight: Font.Bold
                            }
                            Text {
                                width: parent.width
                                text: statTile.modelData.label
                                color: JobCrushTheme.primaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: statTile.modelData.explanation
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------
            // The funnel.
            // ----------------------------------------------------------
            Rectangle {
                width: parent.width
                height: funnelColumn.implicitHeight + 36
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.width: 1
                border.color: JobCrushTheme.hairlineBorderColor

                Column {
                    id: funnelColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        text: "HOW FAR THEY GOT"
                        color: JobCrushTheme.accentColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1
                    }

                    Text {
                        width: parent.width
                        text: "These are levels. Each one includes the ones before it, so a "
                              + "job that reached Interview also counts as Applied. A job stays "
                              + "counted as applied even after it closes."
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.WordWrap
                        bottomPadding: 4
                    }

                    Repeater {
                        model: statsPage.jobSearchStatsViewModel.funnelSteps

                        delegate: Item {
                            id: funnelRow

                            required property var modelData

                            width: funnelColumn.width
                            height: 30

                            Text {
                                id: funnelRowLabel
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 110
                                text: funnelRow.modelData.displayName
                                color: JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                            }

                            // The empty track, so a level with zero jobs
                            // still shows as a row.
                            Rectangle {
                                id: funnelTrack
                                anchors.left: funnelRowLabel.right
                                anchors.right: funnelRowCount.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                height: 18
                                radius: 4
                                color: JobCrushTheme.appBackgroundColor

                                // The filled bar. shareOfWidest is 0 to 1,
                                // where 1 is the level with the most jobs.
                                // A level with even one job gets a sliver,
                                // so it never looks the same as zero.
                                Rectangle {
                                    readonly property int shortestVisibleBarWidth: 4

                                    height: parent.height
                                    width: Math.max(parent.width * funnelRow.modelData.shareOfWidest,
                                                    funnelRow.modelData.count > 0
                                                        ? shortestVisibleBarWidth : 0)
                                    radius: 4
                                    // The stage's color from the theme. Saved,
                                    // and anything unknown, get the Saved color.
                                    color: {
                                        if (funnelRow.modelData.stageName === "applied")
                                            return JobCrushTheme.stageAppliedColor
                                        if (funnelRow.modelData.stageName === "interview")
                                            return JobCrushTheme.stageInterviewColor
                                        if (funnelRow.modelData.stageName === "offer")
                                            return JobCrushTheme.stageOfferColor
                                        return JobCrushTheme.stageSavedColor
                                    }

                                    Behavior on width {
                                        NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                                    }
                                }
                            }

                            Text {
                                id: funnelRowCount
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40
                                horizontalAlignment: Text.AlignRight
                                text: funnelRow.modelData.count
                                color: JobCrushTheme.primaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------
            // Effort, week by week.
            // ----------------------------------------------------------
            Rectangle {
                width: parent.width
                height: weeklyColumn.implicitHeight + 36
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.width: 1
                border.color: JobCrushTheme.hairlineBorderColor

                Column {
                    id: weeklyColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        text: "WEEK BY WEEK"
                        color: JobCrushTheme.accentColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1
                    }

                    Row {
                        spacing: 16

                        Row {
                            spacing: 6
                            Rectangle {
                                width: 10; height: 10; radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: JobCrushTheme.stageSavedColor
                            }
                            Text {
                                text: "crushed"
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                        Row {
                            spacing: 6
                            Rectangle {
                                width: 10; height: 10; radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: JobCrushTheme.stageAppliedColor
                            }
                            Text {
                                text: "sent"
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                    }

                    // Two bars per week, side by side: jobs crushed and jobs
                    // sent. The bars are scaled so the busiest week fills the
                    // chart.
                    //
                    // The steps for one bar's height: take the count, divide
                    // by the busiest week's count to get a share from 0 to 1,
                    // and multiply by the tallest a bar may be.
                    Item {
                        id: weeklyChart
                        width: weeklyColumn.width
                        height: 150

                        // At least 1, so an empty six months never divides
                        // by zero.
                        readonly property int tallestBarValue:
                            Math.max(1, statsPage.jobSearchStatsViewModel.busiestWeekCount)

                        // The chart's height, less 30 pixels: room below the
                        // bars for the date labels, and a little room above.
                        readonly property real tallestBarHeight: weeklyChart.height - 30

                        // A date under every fourth week. All 26 would run
                        // into each other.
                        readonly property int weeksBetweenDateLabels: 4

                        Row {
                            anchors.fill: parent
                            spacing: 0

                            Repeater {
                                model: statsPage.jobSearchStatsViewModel.weeksOfEffort

                                delegate: Item {
                                    id: weekColumnItem

                                    required property var modelData
                                    required property int index

                                    // One equal slice of the chart per week.
                                    // At least 1, so an empty list never
                                    // divides by zero.
                                    width: weeklyChart.width / Math.max(1,
                                        statsPage.jobSearchStatsViewModel.weeksOfEffort.length)
                                    height: weeklyChart.height

                                    // The baseline, drawn per column so empty
                                    // weeks still show a line.
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 18
                                        width: parent.width
                                        height: 1
                                        color: JobCrushTheme.hairlineBorderColor
                                    }

                                    // The two bars, standing on the baseline.
                                    // Each bar is half the week's slice, less
                                    // 6 pixels shared between the 2-pixel gap
                                    // inside the pair and the space between
                                    // weeks. Never thinner than 3 pixels, so
                                    // a bar can always be seen.
                                    Row {
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 19
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        spacing: 2

                                        Rectangle {
                                            width: Math.max(3, (weekColumnItem.width - 6) / 2)
                                            height: weekColumnItem.modelData.crushedCount
                                                    * weeklyChart.tallestBarHeight
                                                    / weeklyChart.tallestBarValue
                                            color: JobCrushTheme.stageSavedColor
                                            radius: 1
                                        }
                                        Rectangle {
                                            width: Math.max(3, (weekColumnItem.width - 6) / 2)
                                            height: weekColumnItem.modelData.appliedCount
                                                    * weeklyChart.tallestBarHeight
                                                    / weeklyChart.tallestBarValue
                                            color: JobCrushTheme.stageAppliedColor
                                            radius: 1
                                        }
                                    }

                                    // A date label every few weeks. All 26
                                    // would be unreadable.
                                    Text {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        visible: weekColumnItem.index
                                                 % weeklyChart.weeksBetweenDateLabels === 0
                                        text: weekColumnItem.modelData.weekLabel
                                        color: JobCrushTheme.mutedTextColor
                                        font.pixelSize: 9
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------
            // The heatmap.
            // ----------------------------------------------------------
            Rectangle {
                id: heatmapPanel

                width: parent.width
                height: heatmapColumn.implicitHeight + 36
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.width: 1
                border.color: JobCrushTheme.hairlineBorderColor

                // The day under the pointer, shown as a line of text below
                // the grid.
                //
                // Not a floating tooltip. A tooltip parented to a cell gets
                // drawn under every cell to its right, so it came out half
                // hidden. A fixed line also cannot be clipped by the panel
                // edge.
                property string hoveredDayText: ""

                Column {
                    id: heatmapColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        text: "DAYS YOU DID SOMETHING"
                        color: JobCrushTheme.accentColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1
                    }

                    Text {
                        width: parent.width
                        text: "Reading counts. Drafting counts. A day spent writing a letter "
                              + "you didn't send is not a blank day."
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.WordWrap
                        bottomPadding: 4
                    }

                    // Seven rows, Monday at the top. One column per week.
                    Row {
                        spacing: 8

                        // Three labels instead of seven. Mon, Wed and Fri
                        // are enough to count rows from.
                        Column {
                            spacing: 3

                            Repeater {
                                model: ["Mon", "", "Wed", "", "Fri", "", ""]

                                delegate: Text {
                                    required property string modelData

                                    height: heatmapGrid.cellSize
                                    width: 26
                                    verticalAlignment: Text.AlignVCenter
                                    text: modelData
                                    color: JobCrushTheme.mutedTextColor
                                    font.pixelSize: 9
                                }
                            }
                        }

                        // One square per day. The days arrive oldest first,
                        // and the grid fills top to bottom, so each column
                        // is one week.
                        Grid {
                            id: heatmapGrid

                            // The square's size, so 26 weeks fit the panel.
                            // The steps: take the panel width, less 40 pixels
                            // for the day labels on the left. Split it into
                            // 26 week columns. Take off the 3-pixel gap
                            // between squares. Then keep the result between
                            // 8 and 14 pixels, so a square is never too
                            // small to point at or too big for the panel.
                            //
                            // 26 is the number of weeks the logic layer
                            // sends (weeksShown in JobSearchStatistics.cpp).
                            readonly property int cellSize: Math.max(8,
                                Math.min(14, (heatmapColumn.width - 40) / 26 - 3))

                            // The most things done on any one day. At least
                            // 1, so an empty six months never divides by
                            // zero.
                            readonly property int busiestDayActivityCount: Math.max(1,
                                statsPage.jobSearchStatsViewModel.busiestDayCount)

                            rows: 7
                            columns: Math.ceil(
                                statsPage.jobSearchStatsViewModel.daysOfEffort.length / 7)
                            flow: Grid.TopToBottom
                            spacing: 3

                            Repeater {
                                model: statsPage.jobSearchStatsViewModel.daysOfEffort

                                delegate: Rectangle {
                                    id: heatmapCell

                                    required property var modelData

                                    width: heatmapGrid.cellSize
                                    height: heatmapGrid.cellSize
                                    radius: 2
                                    color: heatmapCell.modelData.activityCount > 0
                                        ? JobCrushTheme.accentColor
                                        : JobCrushTheme.appBackgroundColor
                                    // Four steps, not a smooth gradient. These
                                    // numbers are too small for a gradient to mean
                                    // anything.
                                    //
                                    // The steps: a day with nothing done is an
                                    // empty square at full strength. Any other
                                    // day is compared to the busiest day. Over
                                    // two thirds of it is full color. Over one
                                    // third is 70 percent. Anything less is
                                    // 40 percent.
                                    opacity: {
                                        if (heatmapCell.modelData.activityCount === 0) return 1.0
                                        const shareOfBusiestDay = heatmapCell.modelData.activityCount
                                                    / heatmapGrid.busiestDayActivityCount
                                        if (shareOfBusiestDay > 0.66) return 1.0
                                        if (shareOfBusiestDay > 0.33) return 0.7
                                        return 0.4
                                    }
                                    border.width: heatmapCell.modelData.activityCount > 0 ? 0 : 1
                                    border.color: JobCrushTheme.hairlineBorderColor

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: heatmapPanel.hoveredDayText =
                                            heatmapCell.modelData.dateLabel + "  —  "
                                            + (heatmapCell.modelData.activityCount === 0
                                                  ? "nothing"
                                                  : heatmapCell.modelData.activityCount === 1
                                                        ? "1 thing"
                                                        : heatmapCell.modelData.activityCount + " things")
                                        onExited: heatmapPanel.hoveredDayText = ""
                                    }

                                }
                            }
                        }
                    }

                    // Keeps its height even when empty, so the panel does not
                    // jump as the pointer moves.
                    Text {
                        width: parent.width
                        height: 16
                        text: heatmapPanel.hoveredDayText
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                    }
                }
            }

            // ----------------------------------------------------------
            // The one number the user controls.
            // ----------------------------------------------------------
            Rectangle {
                width: parent.width
                height: 78
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.width: 1
                border.color: JobCrushTheme.hairlineBorderColor

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    spacing: 14

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: statsPage.jobSearchStatsViewModel.timeToApplyText
                        color: JobCrushTheme.accentColor
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            text: "from crushing a job to sending it"
                            color: JobCrushTheme.primaryTextColor
                            font.pixelSize: JobCrushTheme.bodyFontSize
                        }
                        Text {
                            text: "the gap you can actually do something about"
                            color: JobCrushTheme.mutedTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                        }
                    }
                }
            }

            Item { width: 1; height: 8 }
        }
    }

    VerticalScrollBar {
        anchors.top: statsScroller.top
        anchors.bottom: statsScroller.bottom
        anchors.right: parent.right
        anchors.rightMargin: 18
        flickableTarget: statsScroller
    }
}
