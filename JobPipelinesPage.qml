import QtQuick

// JobPipelinesPage
//
// The board. Five columns, and cards you drag between them.
//
// Plural on purpose, and the plural is the point: a job search is not one
// funnel you feed and then sit beside. It is a dozen of them running at
// different speeds, and the reason to look at this screen is to see that you
// have feelers out in every direction rather than one hope you are guarding.
//
// Pure view. It knows about columns, colors and dragging; it knows nothing
// about what a stage means or what moving between them costs. That is
// JobPipelines' business, one layer down.
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

                width: (boardColumns.width - (jobPipelinesPage.stageNames.length - 1) * 12)
                       / jobPipelinesPage.stageNames.length
                height: boardColumns.height
                radius: 10
                color: JobCrushTheme.panelBackgroundColor

                // The column lights up while a card is over it — the whole
                // reason a drag feels safe is that the target says "yes, here"
                // before you let go.
                border.width: columnDropArea.containsDrag ? 2 : 1
                border.color: columnDropArea.containsDrag
                    ? jobPipelinesPage.stageColorFor(stageColumn.stageName)
                    : JobCrushTheme.hairlineBorderColor

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
                }

                // ---- What this column is for ----
                //
                // Only while it is empty. A heading with nothing under it is a
                // question; this answers it before anybody has to ask.
                Text {
                    anchors.top: columnHeading.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    visible: stageColumn.cardsHere.length === 0
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

                    WheelHandler {
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: function(wheelEvent) {
                            const scrollableDistance = Math.max(
                                0, columnScroller.contentHeight - columnScroller.height)
                            if (scrollableDistance <= 0) {
                                return
                            }
                            columnScroller.contentY = Math.max(0, Math.min(
                                scrollableDistance,
                                columnScroller.contentY - (wheelEvent.angleDelta.y / 120) * 240))
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

                                    // Width comes from the SLOT, not from the
                                    // parent, because this card leaves its
                                    // parent while it is being dragged (see
                                    // the handler below) and a card that
                                    // changes width the moment you pick it up
                                    // feels broken in the hand.
                                    width: cardSlot.width
                                    height: cardBody.implicitHeight + 22
                                    radius: 8
                                    color: JobCrushTheme.cardBackgroundColor
                                    border.width: cardDragHandler.active ? 2 : 1
                                    border.color: cardDragHandler.active
                                        ? jobPipelinesPage.stageColorFor(stageColumn.stageName)
                                        : JobCrushTheme.hairlineBorderColor

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
                                                // Out of the column and onto
                                                // the page for the duration of
                                                // the drag.
                                                //
                                                // Its column CLIPS — it has to,
                                                // or a long list would spill
                                                // over the one below it — and a
                                                // card that vanishes the moment
                                                // it crosses the column edge is
                                                // a card nobody will ever
                                                // succeed in dragging. So the
                                                // card is handed to the page,
                                                // where nothing clips it, and
                                                // handed back on release. The
                                                // empty slot stays behind and
                                                // holds the space.
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

                                            // Let go. drop() has to be called
                                            // while Drag.active is still true —
                                            // clearing it first ends the drag
                                            // as a CANCEL, and the card silently
                                            // springs back no matter how
                                            // carefully it was aimed. That is
                                            // the whole bug, and it looks
                                            // exactly like a broken column.
                                            targetedJobCard.Drag.drop()
                                            targetedJobCard.Drag.active = false

                                            targetedJobCard.parent = cardSlot
                                            targetedJobCard.x = 0
                                            targetedJobCard.y = 0
                                            jobPipelinesPage.draggingJobApplicationId = -1
                                        }
                                    }

                                    // Clicking opens the card. A column is a
                                    // couple of hundred pixels wide, and a
                                    // notes box on every card at that width
                                    // turns five columns into wallpaper. Shut,
                                    // a card answers "which job, where, how
                                    // far along". Open, it answers everything
                                    // else.
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

                                        // Shut, a note announces itself in one
                                        // line rather than hiding completely —
                                        // otherwise the thing you wrote down
                                        // is the thing you forget you wrote.
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

                                            // Grows to fit, so a long note is
                                            // a note you can actually read.
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

                                            // Stacked rather than side by side.
                                            // A column is narrow, and two links
                                            // on one line means one of them is
                                            // always cut in half.
                                            Text {
                                                width: parent.width
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
    // Says what to do, not merely that there is nothing here. An empty state
    // that only reports emptiness is a dead end with a nice font.
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
