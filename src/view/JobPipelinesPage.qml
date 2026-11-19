import QtQuick
import JobCrush

// JobPipelinesPage
//
// The board. Six columns, and cards you drag between them. The last one,
// Escaped, starts folded to a thin strip at the right edge.
//
// The name is plural on purpose. A job search is many leads at once, and
// each one moves at its own speed. This screen shows the user how many
// leads they have out.
//
// This is the view layer. It knows columns, colors and dragging. It does
// not know what a stage means or what a move between stages does. The
// JobPipelines class in the logic layer decides that.
Rectangle {
    id: jobPipelinesPage

    // Injected from Main.
    property JobPipelineBoardViewModel jobPipelineBoardViewModel

    color: JobCrushTheme.appBackgroundColor

    // The card currently in the air, if any. Held here rather than on a
    // column, because a drag starts in one column and ends in another.
    property int draggingJobApplicationId: -1

    // A small button on a card, as wide as its own words. These used to be
    // underlined text. People click buttons, not words: the underlined text
    // did not read as something to press.
    component CardButton: Rectangle {
        id: cardButton

        property string labelText: ""
        property color edgeColor: JobCrushTheme.noticeTextColor

        signal pressed

        width: cardButtonLabel.implicitWidth + 20
        height: 26
        radius: 6
        color: cardButtonMouseArea.containsMouse ? cardButton.edgeColor : "transparent"
        border.width: 1
        border.color: cardButton.edgeColor

        Text {
            id: cardButtonLabel
            anchors.centerIn: parent
            text: cardButton.labelText
            textFormat: Text.PlainText
            color: cardButtonMouseArea.containsMouse
                ? JobCrushTheme.onAccentTextColor : cardButton.edgeColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: cardButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: cardButton.pressed()
        }
    }

    // A sort button at the top of a column. As wide as its own words. The
    // button whose sort the column is using now is filled in with the
    // column's color. The others are only outlined.
    component SortButton: Rectangle {
        id: sortButton

        property string labelText: ""
        property bool isInUse: false
        property color stageColor: JobCrushTheme.hairlineBorderColor

        // True while the mouse is on this button. The column reads it to show
        // the button's hover line.
        readonly property bool isHovered: sortButtonMouseArea.containsMouse

        signal pressed

        width: sortButtonLabel.implicitWidth + 14
        height: 22
        radius: 5
        color: sortButton.isInUse ? sortButton.stageColor : "transparent"
        border.width: 1
        border.color: sortButton.isInUse || sortButtonMouseArea.containsMouse
            ? sortButton.stageColor : JobCrushTheme.hairlineBorderColor

        Text {
            id: sortButtonLabel
            anchors.centerIn: parent
            text: sortButton.labelText
            textFormat: Text.PlainText
            color: sortButton.isInUse ? JobCrushTheme.onAccentTextColor
                                      : JobCrushTheme.secondaryTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: sortButton.isInUse ? Font.DemiBold : Font.Normal
        }

        MouseArea {
            id: sortButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sortButton.pressed()
        }
    }

    readonly property list<string> stageNames:
        jobPipelineBoardViewModel ? jobPipelineBoardViewModel.stageNamesInBoardOrder() : []

    // A column that starts folded (Escaped) shows as a thin strip at the
    // right edge until the user clicks it. This is false while it is
    // folded. It is not saved, so the column is folded again every time Job
    // Crush opens.
    property bool foldedColumnIsOpen: false

    // How wide a folded column's strip is.
    readonly property int foldedColumnWidth: 40

    // How many columns are folded right now. The other columns share the
    // room those strips do not use.
    readonly property int foldedColumnCount: {
        if (jobPipelinesPage.foldedColumnIsOpen || !jobPipelineBoardViewModel) {
            return 0
        }
        let count = 0
        for (const stageName of jobPipelinesPage.stageNames) {
            if (jobPipelineBoardViewModel.stageStartsFolded(stageName)) {
                ++count
            }
        }
        return count
    }

    // The names of the three sort buttons each column has: "date", "title"
    // and "company". The viewmodel gives the words shown on each one.
    readonly property list<string> sortChoiceNames:
        jobPipelineBoardViewModel ? jobPipelineBoardViewModel.sortChoiceNames() : []

    // Each stage has its own color in the theme. The names here are the
    // storage names from the viewmodel, not the words shown on screen. An
    // unknown name gets the plain border color.
    function stageColorFor(stageName) {
        switch (stageName) {
        case "saved":     return JobCrushTheme.stageSavedColor
        case "applied":   return JobCrushTheme.stageAppliedColor
        case "interview": return JobCrushTheme.stageInterviewColor
        case "offer":     return JobCrushTheme.stageOfferColor
        case "closed":    return JobCrushTheme.stageClosedColor
        case "escaped":   return JobCrushTheme.mutedTextColor
        }
        return JobCrushTheme.hairlineBorderColor
    }

    // ------------------------------------------------------------------
    // Header
    // ------------------------------------------------------------------
    Item {
        id: boardHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // Tall enough for the message banner when the message runs to three
        // lines. Normally 78.
        height: Math.max(78, lastActionBanner.height + 16)

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Job Pipelines"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.titleFontSize
                font.weight: Font.DemiBold
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: jobPipelinesPage.jobPipelineBoardViewModel.totalCardCount === 1
                    ? "1 job in play"
                    : jobPipelinesPage.jobPipelineBoardViewModel.totalCardCount
                      + " jobs in play"
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
            }

            // Asks each employer whether these jobs are still listed. Only
            // when pressed: Job Crush never calls other people's servers on
            // its own.
            Rectangle {
                id: stillListedCheckButton

                readonly property bool checkIsRunning:
                    jobPipelinesPage.jobPipelineBoardViewModel.stillListedCheckIsRunning

                anchors.verticalCenter: parent.verticalCenter
                visible: jobPipelinesPage.jobPipelineBoardViewModel.totalCardCount > 0
                width: stillListedCheckLabel.implicitWidth + 24
                height: 30
                radius: 8
                color: stillListedCheckMouseArea.containsMouse && !checkIsRunning
                    ? JobCrushTheme.panelBackgroundColor : "transparent"
                border.width: 1
                border.color: JobCrushTheme.hairlineBorderColor

                Text {
                    id: stillListedCheckLabel
                    anchors.centerIn: parent
                    text: "Are these still open?"
                    color: stillListedCheckButton.checkIsRunning
                        ? JobCrushTheme.mutedTextColor : JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                MouseArea {
                    id: stillListedCheckMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !stillListedCheckButton.checkIsRunning
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: jobPipelinesPage.jobPipelineBoardViewModel
                        .checkWhichJobsAreStillListed()
                }
            }

            // What the check is doing, or what it found. Real numbers, no
            // spinner.
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0
                // Wraps instead of cutting off. The sentence says how many
                // jobs are gone, and a number the user cannot read is no
                // better than no number.
                width: 460
                text: jobPipelinesPage.jobPipelineBoardViewModel.stillListedCheckText
                textFormat: Text.PlainText
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.WordWrap
            }
        }

        // What just happened. Sits up here rather than popping over the board,
        // because the thing the user is looking at IS the board.
        Rectangle {
            id: lastActionBanner

            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            visible: jobPipelinesPage.jobPipelineBoardViewModel.lastActionText.length > 0
            // A fixed width, and the height follows the text. The sentence
            // wraps and is always readable from start to finish. A cut-off
            // sentence is worse than no sentence: the user can see that
            // something happened and cannot read what.
            width: 520
            height: Math.max(36, lastActionLabel.implicitHeight + 20)
            radius: 8
            color: JobCrushTheme.panelBackgroundColor
            border.width: 1
            border.color: JobCrushTheme.hairlineBorderColor

            Text {
                id: lastActionLabel
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: dismissLastActionLabel.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: jobPipelinesPage.jobPipelineBoardViewModel.lastActionText
                textFormat: Text.PlainText
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.WordWrap
            }

            Text {
                id: dismissLastActionLabel
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "×"
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: jobPipelinesPage.jobPipelineBoardViewModel.clearLastAction()
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // The columns
    // ------------------------------------------------------------------
    Row {
        id: boardColumns
        anchors.top: boardHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.bottomMargin: 20
        spacing: 12
        visible: jobPipelinesPage.jobPipelineBoardViewModel.totalCardCount > 0

        Repeater {
            model: jobPipelinesPage.stageNames

            delegate: Rectangle {
                id: stageColumn

                required property string modelData
                readonly property string stageName: modelData

                // Reading boardRevision makes every binding in the column
                // re-evaluate the moment a card moves anywhere on the board.
                readonly property var cardsHere: {
                    jobPipelinesPage.jobPipelineBoardViewModel.boardRevision
                    return jobPipelinesPage.jobPipelineBoardViewModel
                        .cardsInStage(stageColumn.stageName)
                }

                // True while this column shows only as a thin strip.
                readonly property bool isFolded:
                    jobPipelinesPage.jobPipelineBoardViewModel.stageStartsFolded(
                        stageColumn.stageName)
                    && !jobPipelinesPage.foldedColumnIsOpen

                // A folded column is a thin strip. Every other column gets an
                // even share of the row's width, less the gaps between
                // columns and less the folded strips.
                width: stageColumn.isFolded
                    ? jobPipelinesPage.foldedColumnWidth
                    : (boardColumns.width
                       - (jobPipelinesPage.stageNames.length - 1) * boardColumns.spacing
                       - jobPipelinesPage.foldedColumnCount * jobPipelinesPage.foldedColumnWidth)
                      / (jobPipelinesPage.stageNames.length - jobPipelinesPage.foldedColumnCount)
                height: boardColumns.height
                radius: 10
                color: JobCrushTheme.panelBackgroundColor

                // The border lights up in the stage's color while a card is
                // over the column. The user sees where the card will land
                // before letting go.
                border.width: columnDropArea.containsDrag ? 2 : 1
                border.color: columnDropArea.containsDrag
                    ? jobPipelinesPage.stageColorFor(stageColumn.stageName)
                    : JobCrushTheme.hairlineBorderColor

                // A glow around the column while a card is over it. Only the
                // Trace On themes draw it; the others set the glow to zero.
                GlowHalo {
                    glowColor: jobPipelinesPage.stageColorFor(stageColumn.stageName)
                    cornerRadius: stageColumn.radius
                    isLit: columnDropArea.containsDrag
                }

                // ---- Column heading ----
                // ---- The folded strip ----
                //
                // While the column is folded, only this shows: an arrow, the
                // count, and the column's name written sideways. Clicking
                // anywhere on the strip opens the column. A card can still be
                // dropped on the strip, because the DropArea at the bottom of
                // the column covers it.
                Item {
                    id: foldedStrip
                    anchors.fill: parent
                    visible: stageColumn.isFolded

                    Column {
                        anchors.top: parent.top
                        anchors.topMargin: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 10

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "‹"
                            color: foldedStripMouseArea.containsMouse
                                ? JobCrushTheme.primaryTextColor
                                : JobCrushTheme.secondaryTextColor
                            font.pixelSize: JobCrushTheme.bodyFontSize + 4
                            font.weight: Font.DemiBold
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: stageColumn.cardsHere.length
                            color: JobCrushTheme.secondaryTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            font.weight: Font.DemiBold
                        }
                    }

                    // The column's name, turned to read from bottom to top.
                    Text {
                        id: foldedStripName
                        anchors.centerIn: parent
                        rotation: -90
                        text: jobPipelinesPage.jobPipelineBoardViewModel
                            .displayLabelForStage(stageColumn.stageName)
                        color: foldedStripMouseArea.containsMouse
                            ? JobCrushTheme.primaryTextColor
                            : JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.2
                    }

                    MouseArea {
                        id: foldedStripMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: jobPipelinesPage.foldedColumnIsOpen = true
                    }
                }

                Item {
                    id: columnHeading
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    height: 26
                    visible: !stageColumn.isFolded

                    Rectangle {
                        id: stageStripe
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 4
                        height: 18
                        radius: 2
                        color: jobPipelinesPage.stageColorFor(stageColumn.stageName)

                        GlowHalo {
                            glowColor: stageStripe.color
                            cornerRadius: stageStripe.radius
                        }
                    }

                    Text {
                        anchors.left: stageStripe.right
                        anchors.leftMargin: 9
                        anchors.verticalCenter: parent.verticalCenter
                        text: jobPipelinesPage.jobPipelineBoardViewModel
                            .displayLabelForStage(stageColumn.stageName)
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.6
                    }

                    Text {
                        id: columnCardCount
                        anchors.right: foldColumnButton.visible
                            ? foldColumnButton.left : parent.right
                        anchors.rightMargin: foldColumnButton.visible ? 8 : 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: stageColumn.cardsHere.length
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                    }

                    // An arrow that folds the column back to its strip. Only
                    // a column that starts folded has one.
                    Text {
                        id: foldColumnButton
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: jobPipelinesPage.jobPipelineBoardViewModel
                            .stageStartsFolded(stageColumn.stageName)
                        text: "›"
                        color: foldColumnMouseArea.containsMouse
                            ? JobCrushTheme.primaryTextColor
                            : JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize + 4
                        font.weight: Font.DemiBold

                        MouseArea {
                            id: foldColumnMouseArea
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: jobPipelinesPage.foldedColumnIsOpen = false
                        }
                    }

                    // Rest the mouse on the heading to see what the column means.
                    HoverHandler {
                        id: columnHeadingHover
                    }
                }

                // ---- Sort buttons ----
                //
                // Three buttons under the heading: date, title and company.
                // Clicking one sorts this column by it. Clicking the one in
                // use flips the order. The viewmodel saves the choice for
                // this column. A Flow puts a button on a second line if the
                // column is too narrow for all three.
                Flow {
                    id: columnSortButtons
                    visible: !stageColumn.isFolded
                    anchors.top: columnHeading.bottom
                    anchors.topMargin: 4
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 6

                    // The name of the sort button the mouse is on: "date",
                    // "title" or "company". Empty when the mouse is on none
                    // of them.
                    property string hoveredSortChoiceName: ""

                    Repeater {
                        model: jobPipelinesPage.sortChoiceNames

                        delegate: SortButton {
                            id: columnSortButton

                            required property string modelData
                            readonly property string sortChoiceName: modelData

                            // Reading boardRevision makes the words and the
                            // fill update the moment the user clicks.
                            labelText: {
                                jobPipelinesPage.jobPipelineBoardViewModel.boardRevision
                                return jobPipelinesPage.jobPipelineBoardViewModel
                                    .sortButtonText(stageColumn.stageName,
                                                    columnSortButton.sortChoiceName)
                            }
                            isInUse: {
                                jobPipelinesPage.jobPipelineBoardViewModel.boardRevision
                                return jobPipelinesPage.jobPipelineBoardViewModel
                                    .sortChoiceIsInUse(stageColumn.stageName,
                                                       columnSortButton.sortChoiceName)
                            }
                            stageColor: jobPipelinesPage.stageColorFor(stageColumn.stageName)

                            onPressed: jobPipelinesPage.jobPipelineBoardViewModel
                                .chooseSortFor(stageColumn.stageName,
                                               columnSortButton.sortChoiceName)

                            // When the mouse comes onto this button, the
                            // column remembers this button's name, and the
                            // box below shows its hover line. When the mouse
                            // leaves, the name is cleared, unless the mouse
                            // has already moved onto another button.
                            onIsHoveredChanged: {
                                if (columnSortButton.isHovered) {
                                    columnSortButtons.hoveredSortChoiceName =
                                        columnSortButton.sortChoiceName
                                } else if (columnSortButtons.hoveredSortChoiceName
                                           === columnSortButton.sortChoiceName) {
                                    columnSortButtons.hoveredSortChoiceName = ""
                                }
                            }
                        }
                    }
                }

                // ---- What a sort button does, while the mouse is on it ----
                Rectangle {
                    anchors.top: columnSortButtons.bottom
                    anchors.topMargin: 4
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    z: 50
                    visible: columnSortButtons.hoveredSortChoiceName !== ""
                             && !stageColumn.isFolded
                    height: sortHoverText.implicitHeight + 16
                    radius: 6
                    color: JobCrushTheme.appBackgroundColor
                    border.width: 1
                    border.color: jobPipelinesPage.stageColorFor(stageColumn.stageName)

                    Text {
                        id: sortHoverText
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        // Reading boardRevision makes the line update when
                        // a click changes what the button does.
                        text: {
                            jobPipelinesPage.jobPipelineBoardViewModel.boardRevision
                            if (columnSortButtons.hoveredSortChoiceName === "") {
                                return ""
                            }
                            return jobPipelinesPage.jobPipelineBoardViewModel
                                .sortButtonHoverText(stageColumn.stageName,
                                                     columnSortButtons.hoveredSortChoiceName)
                        }
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }
                }

                // ---- What the column means, while the mouse is on the heading ----
                Rectangle {
                    id: columnMeaningBox
                    anchors.top: columnHeading.bottom
                    anchors.topMargin: 4
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    z: 50
                    visible: columnHeadingHover.hovered && !stageColumn.isFolded
                    height: columnMeaningText.implicitHeight + 16
                    radius: 6
                    color: JobCrushTheme.appBackgroundColor
                    border.width: 1
                    border.color: jobPipelinesPage.stageColorFor(stageColumn.stageName)

                    Text {
                        id: columnMeaningText
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: jobPipelinesPage.jobPipelineBoardViewModel
                            .hoverLineForStage(stageColumn.stageName)
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }
                }

                // ---- What this column is for ----
                //
                // Shown only while the column is empty, so a new user knows
                // what belongs here. Hidden while the hover box is open,
                // because the two would overlap.
                Text {
                    anchors.top: columnSortButtons.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    visible: stageColumn.cardsHere.length === 0
                             && !columnHeadingHover.hovered
                             && !stageColumn.isFolded
                    text: jobPipelinesPage.jobPipelineBoardViewModel
                        .explanationForStage(stageColumn.stageName)
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }

                // ---- The cards ----
                Flickable {
                    id: columnScroller
                    visible: !stageColumn.isFolded
                    anchors.top: columnSortButtons.bottom
                    anchors.topMargin: 8
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.bottomMargin: 10
                    contentWidth: width
                    contentHeight: cardStack.implicitHeight
                    clip: true

                    // One notch of the mouse wheel scrolls about one card.
                    //
                    // The steps: find the height of the shortest card in the
                    // column, plus the gap under it. That is one step. An
                    // open card is taller than a shut one, so the shortest
                    // card is used, which is a shut card. Then turn the
                    // wheel's raw count into notches, turn notches into
                    // steps, move the view up or down by that much, and keep
                    // the result between the top and the bottom of the
                    // cards. A column whose cards all fit does not scroll.
                    WheelHandler {
                        // Qt reports one wheel notch as 120 units of angleDelta.
                        readonly property real angleDeltaPerNotch: 120

                        // Used only if the column has no card to measure.
                        readonly property real fallbackPixelsPerNotch: 120

                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: function(wheelEvent) {
                            const scrollableDistance = Math.max(
                                0, columnScroller.contentHeight - columnScroller.height)
                            if (scrollableDistance <= 0) {
                                return
                            }

                            // cardStack.children holds the card slots and the
                            // Repeater. The Repeater has no height, so it is
                            // skipped by the height check.
                            let shortestCardHeight = 0
                            for (let childIndex = 0; childIndex < cardStack.children.length;
                                 ++childIndex) {
                                const cardHeight = cardStack.children[childIndex].height
                                if (cardHeight > 0 && (shortestCardHeight === 0
                                                       || cardHeight < shortestCardHeight)) {
                                    shortestCardHeight = cardHeight
                                }
                            }
                            const pixelsPerNotch = shortestCardHeight > 0
                                ? shortestCardHeight + cardStack.spacing
                                : fallbackPixelsPerNotch

                            columnScroller.contentY = Math.max(0, Math.min(
                                scrollableDistance,
                                columnScroller.contentY
                                    - (wheelEvent.angleDelta.y / angleDeltaPerNotch)
                                      * pixelsPerNotch))
                        }
                    }

                    Column {
                        id: cardStack
                        width: columnScroller.width
                        spacing: 8

                        Repeater {
                            model: stageColumn.cardsHere

                            delegate: Item {
                                id: cardSlot

                                required property var modelData
                                readonly property var card: modelData

                                width: cardStack.width
                                height: targetedJobCard.height

                                // ---- The card ----
                                Rectangle {
                                    id: targetedJobCard

                                    // The page, held by value from the start.
                                    // Dropping a card into another column
                                    // reloads the list and destroys this
                                    // card's delegate, and the release
                                    // handler below still runs after that.
                                    // By then the name jobPipelinesPage can
                                    // no longer be looked up from in here
                                    // ("ReferenceError: jobPipelinesPage is
                                    // not defined"). A property set at
                                    // creation keeps its value.
                                    readonly property Item boardPage: jobPipelinesPage

                                    // Width comes from the SLOT, not from the
                                    // parent. The card leaves its parent
                                    // while it is dragged (see the handler
                                    // below). Tied to the parent, it would
                                    // change width the moment it was picked
                                    // up.
                                    width: cardSlot.width
                                    height: cardBody.implicitHeight + 22
                                    radius: 8
                                    color: JobCrushTheme.cardBackgroundColor
                                    // An amber edge marks a job whose
                                    // posting is gone from the employer's
                                    // board, so it stands out on the board.
                                    border.width: cardDragHandler.active
                                                  || cardSlot.card.postingSeemsGone ? 2 : 1
                                    border.color: cardDragHandler.active
                                        ? jobPipelinesPage.stageColorFor(stageColumn.stageName)
                                        : (cardSlot.card.postingSeemsGone
                                               ? JobCrushTheme.noticeTextColor
                                               : JobCrushTheme.hairlineBorderColor)

                                    // Lifts while it is in the air, so it
                                    // reads as picked up rather than stuck.
                                    scale: cardDragHandler.active ? 1.03 : 1.0
                                    opacity: cardDragHandler.active ? 0.92 : 1.0
                                    z: cardDragHandler.active ? 50 : 1

                                    Drag.source: targetedJobCard

                                    // Says what this drag IS, so the ProDocs
                                    // file basket that fills the window knows
                                    // it is not for them. Without it, moving a
                                    // card raises the basket and the drop
                                    // never reaches the column.
                                    Drag.keys: ["jobCrushBoardCard"]

                                    // The point of the card that picks the
                                    // column: its middle, near the top, where
                                    // the title is.
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: 24

                                    // What the DropArea underneath reads when
                                    // the card lands on it.
                                    property int jobApplicationId: cardSlot.card.jobApplicationId

                                    DragHandler {
                                        id: cardDragHandler
                                        target: targetedJobCard

                                        onActiveChanged: {
                                            if (active) {
                                                // Picked up. Move the card out
                                                // of its column and onto the
                                                // page until it is let go.
                                                //
                                                // The column CLIPS. It must,
                                                // or a long list would spill
                                                // past its bottom edge. But a
                                                // clipped card disappears as
                                                // soon as it crosses the
                                                // column edge, so it could
                                                // never reach another column.
                                                // The page does not clip, so
                                                // the card rides on the page
                                                // and goes back on release.
                                                // The empty slot stays behind
                                                // and holds the space.
                                                //
                                                // The steps: find where the
                                                // card sits in page terms,
                                                // give it to the page, put it
                                                // at that same spot so it
                                                // does not jump, start the
                                                // drag, and remember which job
                                                // is in the air.
                                                const placeOnThePage =
                                                    targetedJobCard.mapToItem(
                                                        jobPipelinesPage, 0, 0)
                                                targetedJobCard.parent = jobPipelinesPage
                                                targetedJobCard.x = placeOnThePage.x
                                                targetedJobCard.y = placeOnThePage.y
                                                targetedJobCard.Drag.active = true
                                                jobPipelinesPage.draggingJobApplicationId
                                                    = cardSlot.card.jobApplicationId
                                                return
                                            }

                                            // Let go. drop() must be called
                                            // while Drag.active is still true.
                                            // Clearing it first ends the drag
                                            // as a CANCEL. Then the card goes
                                            // back to where it started, no
                                            // matter where it was dropped, and
                                            // the column looks broken.
                                            //
                                            // After the drop, the card goes
                                            // back into its slot at the top
                                            // left. boardPage is used, not
                                            // jobPipelinesPage, for the reason
                                            // given at boardPage above.
                                            targetedJobCard.Drag.drop()
                                            targetedJobCard.Drag.active = false

                                            targetedJobCard.parent = cardSlot
                                            targetedJobCard.x = 0
                                            targetedJobCard.y = 0
                                            targetedJobCard.boardPage.draggingJobApplicationId = -1
                                        }
                                    }

                                    // Clicking opens the card, and clicking
                                    // again shuts it. A column is a couple of
                                    // hundred pixels wide. A notes box on
                                    // every card at that width would fill the
                                    // board with text. Shut, a card shows
                                    // which job, where, and how far along.
                                    // Open, it shows the notes, the dates and
                                    // the links.
                                    TapHandler {
                                        onTapped: targetedJobCard.isOpen = !targetedJobCard.isOpen
                                    }

                                    property bool isOpen: false

                                    Column {
                                        id: cardBody
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.margins: 11
                                        spacing: 4

                                        Text {
                                            width: parent.width
                                            text: cardSlot.card.positionTitle
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.primaryTextColor
                                            font.pixelSize: JobCrushTheme.bodyFontSize
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.Wrap
                                            maximumLineCount: targetedJobCard.isOpen ? 4 : 2
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            width: parent.width
                                            text: cardSlot.card.companyName
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.secondaryTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            width: parent.width
                                            visible: text.length > 0
                                            text: {
                                                const place = cardSlot.card.isRemoteRole
                                                    ? "Remote"
                                                    : cardSlot.card.locationText
                                                const applied = cardSlot.card.appliedText.length > 0
                                                    ? "sent " + cardSlot.card.appliedText
                                                    : ""
                                                if (place.length > 0 && applied.length > 0) {
                                                    return place + "  ·  " + applied
                                                }
                                                return place.length > 0 ? place : applied
                                            }
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.mutedTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            elide: Text.ElideRight
                                        }

                                        // The posting has gone from the
                                        // employer's board. One line shut;
                                        // the full sentence and a search when
                                        // the card is open.
                                        Text {
                                            width: parent.width
                                            visible: cardSlot.card.postingSeemsGone
                                            text: targetedJobCard.isOpen
                                                ? cardSlot.card.postingGoneText
                                                : "Posting seems to be gone"
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.noticeTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            wrapMode: targetedJobCard.isOpen
                                                ? Text.Wrap : Text.NoWrap
                                            elide: Text.ElideRight
                                        }

                                        // What to do about a job that looks
                                        // gone. Two or three buttons, side by side,
                                        // wrapping to a second row on a narrow
                                        // card. "Mark this as still open" is
                                        // the user's word, and their word
                                        // wins: the amber mark comes off and
                                        // stays off.
                                        Flow {
                                            width: parent.width
                                            visible: cardSlot.card.postingSeemsGone
                                                     && targetedJobCard.isOpen
                                            spacing: 8
                                            topPadding: 2
                                            bottomPadding: 2

                                            CardButton {
                                                labelText: "Search the web for it"
                                                onPressed: jobPipelinesPage
                                                    .jobPipelineBoardViewModel
                                                    .searchTheWebForJob(
                                                        cardSlot.card.jobApplicationId)
                                            }

                                            CardButton {
                                                labelText: "Mark this as still open"
                                                onPressed: jobPipelinesPage
                                                    .jobPipelineBoardViewModel
                                                    .userSaysJobIsStillOpen(
                                                        cardSlot.card.jobApplicationId)
                                            }

                                            // Only on a card still in Crushed.
                                            // The job was never applied to,
                                            // and now the posting is gone.
                                            CardButton {
                                                visible: cardSlot.card.canMoveToEscaped
                                                labelText: "Move to Escaped"
                                                onPressed: jobPipelinesPage
                                                    .jobPipelineBoardViewModel
                                                    .moveCardToStage(
                                                        cardSlot.card.jobApplicationId,
                                                        "escaped")
                                            }
                                        }

                                        // Job Crush has no link for this job
                                        // that it can check. The user has to
                                        // check it themselves, so the card
                                        // says so. Without this line the card
                                        // looks the same as a card that was
                                        // checked and found still listed.
                                        Text {
                                            width: parent.width
                                            visible: cardSlot.card.cannotBeChecked
                                                     && targetedJobCard.isOpen
                                            text: "Job Crush can't check this one. The "
                                                  + "link isn't to a job board it can read."
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.mutedTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            wrapMode: Text.WordWrap
                                        }

                                        // Shown on an open card once the user
                                        // has said the job is still open.
                                        Text {
                                            width: parent.width
                                            visible: cardSlot.card.userSaysStillOpen
                                                     && targetedJobCard.isOpen
                                            text: cardSlot.card.userSaysStillOpenText
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.mutedTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            wrapMode: Text.Wrap
                                        }

                                        // On a shut card, the first line of the
                                        // note still shows. If it were hidden,
                                        // the user could forget the note was
                                        // there.
                                        Text {
                                            width: parent.width
                                            visible: !targetedJobCard.isOpen
                                                     && cardSlot.card.notesText.length > 0
                                            text: "\u270E  " + cardSlot.card.notesText
                                            textFormat: Text.PlainText
                                            color: JobCrushTheme.secondaryTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            elide: Text.ElideRight
                                        }

                                        // ---- Open ----
                                        Column {
                                            width: parent.width
                                            visible: targetedJobCard.isOpen
                                            spacing: 8

                                            Item { width: 1; height: 2 }

                                            // Grows to fit a long note, up to a
                                            // set height. Past that it scrolls.
                                            InlineEditField {
                                                width: parent.width
                                                labelText: "Notes"
                                                placeholderText: "who you spoke to…"
                                                allowsMultipleLines: true
                                                tallestBeforeItScrolls: 220
                                                fieldText: cardSlot.card.notesText
                                                onEditingCommitted: function(newText) {
                                                    jobPipelinesPage.jobPipelineBoardViewModel
                                                        .setNotesFor(
                                                            cardSlot.card.jobApplicationId,
                                                            newText)
                                                }
                                            }

                                            // The day it was sent. Dragging to
                                            // Applied stamps today, and people
                                            // do not always drag on the day
                                            // they apply. Their date beats the
                                            // one the app guessed.
                                            InlineEditField {
                                                id: appliedDateField
                                                width: parent.width
                                                visible: cardSlot.card.appliedDateIsEditable
                                                labelText: "Applied on"
                                                placeholderText: "10/8, 8 Oct, yesterday…"
                                                fieldText: cardSlot.card.appliedDateEditText
                                                onEditingCommitted: function(newText) {
                                                    // Refused: the box goes back to
                                                    // the stored date, and the
                                                    // sentence at the top says why.
                                                    //
                                                    // Accepted: do not touch the box.
                                                    // The board has reloaded and this
                                                    // card may already be gone, with a
                                                    // fresh one showing the new date.
                                                    const dateWasSaved =
                                                        jobPipelinesPage.jobPipelineBoardViewModel
                                                            .setAppliedDateFor(
                                                                cardSlot.card.jobApplicationId,
                                                                newText)
                                                    if (!dateWasSaved) {
                                                        appliedDateField.showStoredText()
                                                    }
                                                }
                                            }

                                            // Every way of reaching this job,
                                            // one link each.
                                            //
                                            // The same opening is often on the
                                            // employer's own board AND on a job
                                            // site. The app cannot know which
                                            // one the user wants. One person
                                            // wants the employer's form.
                                            // Another already has an account
                                            // on a site. A third wants to see
                                            // if the job is still open. So all
                                            // of them are shown and the user
                                            // picks.
                                            //
                                            // Stacked rather than side by side.
                                            // A column is narrow, and two links
                                            // on one line means one of them is
                                            // always cut in half.
                                            Repeater {
                                                model: cardSlot.card.postingRoutes

                                                delegate: Text {
                                                    id: postingRouteLink

                                                    required property var modelData

                                                    width: parent.width
                                                    text: "see this one on "
                                                          + postingRouteLink.modelData.boardDisplayName
                                                    textFormat: Text.PlainText
                                                    color: postingRouteMouseArea.containsMouse
                                                        ? JobCrushTheme.accentColor
                                                        : JobCrushTheme.mutedTextColor
                                                    font.pixelSize: JobCrushTheme.smallFontSize
                                                    font.underline: postingRouteMouseArea.containsMouse
                                                    elide: Text.ElideRight

                                                    MouseArea {
                                                        id: postingRouteMouseArea
                                                        anchors.fill: parent
                                                        anchors.margins: -3
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: jobPipelinesPage
                                                            .jobPipelineBoardViewModel
                                                            .openPostingRouteInBrowser(
                                                                postingRouteLink.modelData.postingUrl)
                                                    }
                                                }
                                            }

                                            // The fallback, for every job
                                            // saved before routes were kept.
                                            // Without it those cards would
                                            // lose their only link.
                                            Text {
                                                width: parent.width
                                                // Hidden when there is no link at
                                                // all, as for a job an AI added
                                                // from a site it gave no link for.
                                                visible: (cardSlot.card.postingRoutes === undefined
                                                          || cardSlot.card.postingRoutes.length === 0)
                                                         && cardSlot.card.sourceUrl !== undefined
                                                         && cardSlot.card.sourceUrl.length > 0
                                                text: "open the posting"
                                                color: openPostingMouseArea.containsMouse
                                                    ? JobCrushTheme.accentColor
                                                    : JobCrushTheme.mutedTextColor
                                                font.pixelSize: JobCrushTheme.smallFontSize
                                                font.underline: openPostingMouseArea.containsMouse

                                                MouseArea {
                                                    id: openPostingMouseArea
                                                    anchors.fill: parent
                                                    anchors.margins: -3
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: jobPipelinesPage
                                                        .jobPipelineBoardViewModel
                                                        .openPostingInBrowser(
                                                            cardSlot.card.jobApplicationId)
                                                }
                                            }

                                            // Looks for the same job on the
                                            // employer's own board and adds it
                                            // as another place to apply.
                                            Text {
                                                width: parent.width
                                                text: "look for this job elsewhere"
                                                color: lookElsewhereMouseArea.containsMouse
                                                    ? JobCrushTheme.accentColor
                                                    : JobCrushTheme.mutedTextColor
                                                font.pixelSize: JobCrushTheme.smallFontSize
                                                font.underline: lookElsewhereMouseArea.containsMouse

                                                MouseArea {
                                                    id: lookElsewhereMouseArea
                                                    anchors.fill: parent
                                                    anchors.margins: -3
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: jobPipelinesPage
                                                        .jobPipelineBoardViewModel
                                                        .lookForOtherPlacesToApply(
                                                            cardSlot.card.jobApplicationId)
                                                }
                                            }

                                            // The job's own folder: its
                                            // JobQuest.txt, resume and cover
                                            // letter.
                                            Text {
                                                width: parent.width
                                                visible: cardSlot.card.jobFolderPath !== undefined
                                                         && cardSlot.card.jobFolderPath.length > 0
                                                text: "open this job's folder"
                                                color: openJobFolderMouseArea.containsMouse
                                                    ? JobCrushTheme.accentColor
                                                    : JobCrushTheme.mutedTextColor
                                                font.pixelSize: JobCrushTheme.smallFontSize
                                                font.underline: openJobFolderMouseArea.containsMouse

                                                MouseArea {
                                                    id: openJobFolderMouseArea
                                                    anchors.fill: parent
                                                    anchors.margins: -3
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: jobPipelinesPage
                                                        .jobPipelineBoardViewModel
                                                        .openJobFolder(
                                                            cardSlot.card.jobApplicationId)
                                                }
                                            }

                                            Text {
                                                width: parent.width
                                                text: "take it off the board"
                                                color: removeCardMouseArea.containsMouse
                                                    ? JobCrushTheme.callToActionColor
                                                    : JobCrushTheme.mutedTextColor
                                                font.pixelSize: JobCrushTheme.smallFontSize
                                                font.underline: removeCardMouseArea.containsMouse

                                                MouseArea {
                                                    id: removeCardMouseArea
                                                    anchors.fill: parent
                                                    anchors.margins: -3
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: jobPipelinesPage
                                                        .jobPipelineBoardViewModel
                                                        .removeCardFromBoard(
                                                            cardSlot.card.jobApplicationId)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- Where a card lands ----
                //
                // Underneath everything, so it catches a drop anywhere in the
                // column rather than only on the exact stack of cards.
                DropArea {
                    id: columnDropArea
                    anchors.fill: parent
                    z: -1

                    // Cards only. A resume dropped on the board goes to
                    // ProDocs, where it belongs.
                    keys: ["jobCrushBoardCard"]

                    onDropped: function(dropEvent) {
                        // The keys above already keep out anything but a
                        // card. This check is a second guard: with no job id
                        // there is nothing to move, so the drop is left alone.
                        if (!dropEvent.source || dropEvent.source.jobApplicationId === undefined) {
                            return
                        }
                        jobPipelinesPage.jobPipelineBoardViewModel.moveCardToStage(
                            dropEvent.source.jobApplicationId, stageColumn.stageName)
                        dropEvent.accept()
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Nothing on the board yet
    //
    // Says what to do next, not only that the board is empty.
    // ------------------------------------------------------------------
    Rectangle {
        anchors.centerIn: parent
        visible: jobPipelinesPage.jobPipelineBoardViewModel.totalCardCount === 0
        width: 460
        height: emptyBoardColumn.implicitHeight + 48
        radius: 12
        color: JobCrushTheme.panelBackgroundColor
        border.width: 1
        border.color: JobCrushTheme.hairlineBorderColor

        Column {
            id: emptyBoardColumn
            anchors.centerIn: parent
            width: parent.width - 56
            spacing: 10

            Text {
                width: parent.width
                text: "Nothing on the board yet"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.titleFontSize
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: "Open Discoveries, find a job worth going after, and press CRUSH. "
                      + "It lands here under Crushed, and from then on you drag it "
                      + "along as things happen — applied, interviewing, offer."
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: "Columns instead of a list because a job search is several "
                      + "things happening at once, and the point of looking is seeing "
                      + "how many."
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.Wrap
            }
        }
    }
}
