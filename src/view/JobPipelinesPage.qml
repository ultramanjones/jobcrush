import QtQuick
import JobCrush

// JobPipelinesPage
//
// The board. Five columns, and cards you drag between them.
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
    property var jobPipelineBoardViewModel

    color: JobCrushTheme.appBackgroundColor

    // The card currently in the air, if any. Held here rather than on a
    // column, because a drag starts in one column and ends in another.
    property int draggingJobApplicationId: -1

    readonly property var stageNames:
        jobPipelineBoardViewModel ? jobPipelineBoardViewModel.stageNamesInBoardOrder() : []

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
        height: 78

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
                width: Math.min(implicitWidth, 460)
                text: jobPipelinesPage.jobPipelineBoardViewModel.stillListedCheckText
                textFormat: Text.PlainText
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                elide: Text.ElideRight
            }
        }

        // What just happened. Sits up here rather than popping over the board,
        // because the thing the user is looking at IS the board.
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            visible: jobPipelinesPage.jobPipelineBoardViewModel.lastActionText.length > 0
            width: Math.min(520, lastActionLabel.implicitWidth + 54)
            height: 36
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
                elide: Text.ElideRight
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

                // The row's width, less the gaps between columns, split evenly.
                width: (boardColumns.width
                        - (jobPipelinesPage.stageNames.length - 1) * boardColumns.spacing)
                       / jobPipelinesPage.stageNames.length
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
                Item {
                    id: columnHeading
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    height: 26

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
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: stageColumn.cardsHere.length
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                    }

                    // Rest the mouse on the heading to see what the column means.
                    HoverHandler {
                        id: columnHeadingHover
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
                    visible: columnHeadingHover.hovered
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
                    anchors.top: columnHeading.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    visible: stageColumn.cardsHere.length === 0
                             && !columnHeadingHover.hovered
                    text: jobPipelinesPage.jobPipelineBoardViewModel
                        .explanationForStage(stageColumn.stageName)
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }

                // ---- The cards ----
                Flickable {
                    id: columnScroller
                    anchors.top: columnHeading.bottom
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

                    // One notch of the mouse wheel scrolls 240 pixels, which
                    // is faster than Qt's default.
                    //
                    // The steps: turn the wheel's raw count into notches,
                    // turn notches into pixels, move the view up or down by
                    // that much, and keep the result between the top and
                    // the bottom of the cards. A column whose cards all fit
                    // does not scroll.
                    WheelHandler {
                        // Qt reports one wheel notch as 120 units of angleDelta.
                        readonly property real angleDeltaPerNotch: 120
                        readonly property real pixelsPerNotch: 240

                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: function(wheelEvent) {
                            const scrollableDistance = Math.max(
                                0, columnScroller.contentHeight - columnScroller.height)
                            if (scrollableDistance <= 0) {
                                return
                            }
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

                                        Text {
                                            width: parent.width
                                            visible: cardSlot.card.postingSeemsGone
                                                     && targetedJobCard.isOpen
                                            text: "search the web for it"
                                            color: searchTheWebMouseArea.containsMouse
                                                ? JobCrushTheme.accentColor
                                                : JobCrushTheme.noticeTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            font.underline: searchTheWebMouseArea.containsMouse

                                            MouseArea {
                                                id: searchTheWebMouseArea
                                                anchors.fill: parent
                                                anchors.margins: -3
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: jobPipelinesPage
                                                    .jobPipelineBoardViewModel
                                                    .searchTheWebForJob(
                                                        cardSlot.card.jobApplicationId)
                                            }
                                        }

                                        // The user checked the job and it is
                                        // still listed. Their word wins: the
                                        // amber mark comes off and stays off.
                                        Text {
                                            width: parent.width
                                            visible: cardSlot.card.postingSeemsGone
                                                     && targetedJobCard.isOpen
                                            text: "It's still open, I checked"
                                            color: stillOpenMouseArea.containsMouse
                                                ? JobCrushTheme.accentColor
                                                : JobCrushTheme.noticeTextColor
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            font.underline: stillOpenMouseArea.containsMouse

                                            MouseArea {
                                                id: stillOpenMouseArea
                                                anchors.fill: parent
                                                anchors.margins: -3
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: jobPipelinesPage
                                                    .jobPipelineBoardViewModel
                                                    .userSaysJobIsStillOpen(
                                                        cardSlot.card.jobApplicationId)
                                            }
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
                text: "Five columns instead of a list because a job search is several "
                      + "things happening at once, and the point of looking is seeing "
                      + "how many."
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.Wrap
            }
        }
    }
}
