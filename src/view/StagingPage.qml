import QtQuick
import JobCrush

// StagingPage
//
// The packet builder. Jobs on the left, the packet on the right, and two
// actions at the bottom: export it as one file, and mark it sent.
//
// Nothing on this page sends anything to anyone. Moonlight writes drafts. The
// user reads them, fixes them, exports the file, and sends it.
Rectangle {
    id: stagingPage

    // Injected from Main.
    property var stagedJobListViewModel
    property var stagingPacketViewModel

    color: JobCrushTheme.appBackgroundColor

    // What the user typed into the box beside the buttons, if anything.
    property string extraInstructionText: ""

    Item {
        id: stagingHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            id: stagingTitle
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "Staging"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        // Anchored to the title, not to a fixed margin. A fixed margin
        // breaks when the font size changes.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: stagingTitle.right
            anchors.leftMargin: 14
            text: "Nothing here is sent. You send it."
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

    // ------------------------------------------------------------------
    // The jobs, down the left.
    // ------------------------------------------------------------------
    Rectangle {
        id: jobColumn
        anchors.top: stagingHeader.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 300
        color: JobCrushTheme.panelBackgroundColor

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: JobCrushTheme.hairlineBorderColor
        }

        Text {
            id: jobColumnHeading
            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 20
            text: "YOUR BOARD"
            color: JobCrushTheme.accentColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }

        // No jobs crushed yet. Say what to do instead of showing a blank
        // list.
        Text {
            anchors.top: jobColumnHeading.bottom
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            visible: stagingPage.stagedJobListViewModel.jobCount === 0
            wrapMode: Text.WordWrap
            text: "No jobs on the board yet.\n\nFind one in Discoveries and hit CRUSH — "
                  + "a packet starts itself the moment you do."
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
        }

        ListView {
            id: jobListView
            anchors.top: jobColumnHeading.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 1
            clip: true
            model: stagingPage.stagedJobListViewModel

            delegate: Rectangle {
                id: jobRow

                required property int index
                required property string companyName
                required property string positionTitle
                required property string pipelineStageName
                required property string fitScoreText
                required property string packetProgressText

                readonly property bool isCurrent:
                    stagingPage.stagedJobListViewModel.jobApplicationIdAt(jobRow.index)
                    === stagingPage.stagingPacketViewModel.currentJobApplicationId

                width: jobListView.width
                height: jobRowColumn.implicitHeight + 24
                color: jobRow.isCurrent
                    ? JobCrushTheme.sidebarSelectedRowColor : "transparent"

                Rectangle {
                    width: 3
                    height: parent.height
                    color: JobCrushTheme.accentColor
                    visible: jobRow.isCurrent
                }

                Column {
                    id: jobRowColumn
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 3

                    Text {
                        width: parent.width
                        text: jobRow.positionTitle
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: jobRow.companyName
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: jobRow.packetProgressText
                              + (jobRow.fitScoreText.length > 0
                                    ? "   ·   " + jobRow.fitScoreText : "")
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: stagingPage.stagingPacketViewModel.showPacketFor(
                        stagingPage.stagedJobListViewModel.jobApplicationIdAt(jobRow.index))
                }
            }
        }

        VerticalScrollBar {
            anchors.top: jobListView.top
            anchors.bottom: jobListView.bottom
            anchors.right: parent.right
            anchors.rightMargin: 6
            flickableTarget: jobListView
        }
    }

    // ------------------------------------------------------------------
    // Nothing chosen yet.
    // ------------------------------------------------------------------
    Text {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: jobColumn.width / 2
        width: 380
        visible: !stagingPage.stagingPacketViewModel.hasSelectedJob
                 && stagingPage.stagedJobListViewModel.jobCount > 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: "Pick a job on the left.\n\nEverything staged for it lands here: what the "
              + "employer asked for, how you match, and the letter — when you ask for it."
        color: JobCrushTheme.mutedTextColor
        font.pixelSize: JobCrushTheme.bodyFontSize
    }

    // ------------------------------------------------------------------
    // The packet.
    // ------------------------------------------------------------------
    Item {
        id: packetPane
        anchors.top: stagingHeader.bottom
        anchors.left: jobColumn.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: stagingPage.stagingPacketViewModel.hasSelectedJob

        // --- Which job, and how it is going ---------------------------
        Item {
            id: packetHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 70

            Column {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.right: parent.right
                anchors.rightMargin: 28
                spacing: 4

                Text {
                    width: parent.width
                    text: stagingPage.stagingPacketViewModel.selectedPositionTitle
                    color: JobCrushTheme.primaryTextColor
                    font.pixelSize: JobCrushTheme.titleFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: stagingPage.stagingPacketViewModel.selectedCompanyName
                          + "   ·   " + stagingPage.stagingPacketViewModel.selectedStageName
                          + (stagingPage.stagingPacketViewModel.selectedFitScoreText.length > 0
                                ? "   ·   " + stagingPage.stagingPacketViewModel.selectedFitScoreText
                                : "")
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }
            }
        }

        // --- What Moonlight can do for this one -----------------------
        Rectangle {
            id: actionBar
            anchors.top: packetHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            height: actionColumn.implicitHeight + 28
            radius: 8
            color: JobCrushTheme.panelBackgroundColor
            border.width: 1
            border.color: JobCrushTheme.hairlineBorderColor

            Column {
                id: actionColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 14
                spacing: 10

                Text {
                    width: parent.width
                    text: stagingPage.stagingPacketViewModel.brainIsAvailable
                        ? "If you have connected an AI brain you can have Moonlight take a run at these."
                        : stagingPage.stagingPacketViewModel.reasonNoBrainIsAvailable
                    color: stagingPage.stagingPacketViewModel.brainIsAvailable
                        ? JobCrushTheme.secondaryTextColor
                        : JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.WordWrap
                }

                Flow {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: [
                            { label: "Draft the cover letter", action: "coverLetter" },
                            { label: "Tailor my resume",       action: "resume" },
                            { label: "Work out the follow-up", action: "followUp" },
                            { label: "Read the posting again", action: "posting" },
                            { label: "Score the fit",          action: "fit" }
                        ]

                        delegate: Rectangle {
                            id: actionButton

                            required property var modelData

                            readonly property bool isAvailable:
                                stagingPage.stagingPacketViewModel.brainIsAvailable
                                && !stagingPage.stagingPacketViewModel.isBusy

                            width: actionButtonLabel.implicitWidth + 26
                            height: 34
                            radius: 6
                            color: actionButton.isAvailable
                                ? (actionButtonMouseArea.containsMouse
                                       ? JobCrushTheme.accentColor
                                       : JobCrushTheme.cardBackgroundColor)
                                : JobCrushTheme.cardBackgroundColor
                            border.width: 1
                            border.color: actionButton.isAvailable
                                ? JobCrushTheme.accentColor
                                : JobCrushTheme.hairlineBorderColor
                            opacity: actionButton.isAvailable ? 1.0 : 0.45

                            Text {
                                id: actionButtonLabel
                                anchors.centerIn: parent
                                text: actionButton.modelData.label
                                color: actionButton.isAvailable
                                    && actionButtonMouseArea.containsMouse
                                    ? JobCrushTheme.onAccentTextColor
                                    : JobCrushTheme.primaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                            }

                            MouseArea {
                                id: actionButtonMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: actionButton.isAvailable
                                cursorShape: actionButton.isAvailable
                                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    const packet = stagingPage.stagingPacketViewModel
                                    const extra = stagingPage.extraInstructionText
                                    if (actionButton.modelData.action === "coverLetter") {
                                        packet.draftCoverLetter(extra)
                                    } else if (actionButton.modelData.action === "resume") {
                                        packet.tailorResume(extra)
                                    } else if (actionButton.modelData.action === "followUp") {
                                        packet.workOutFollowUp(extra)
                                    } else if (actionButton.modelData.action === "posting") {
                                        packet.readThePostingAgain()
                                    } else {
                                        packet.scoreTheFit()
                                    }
                                }
                            }
                        }
                    }
                }

                // Extra instructions from the user. These override the
                // app's built-in instructions, and the AI is told that.
                Rectangle {
                    width: parent.width
                    height: 34
                    radius: 6
                    color: JobCrushTheme.appBackgroundColor
                    border.width: extraInstructionInput.activeFocus ? 2 : 1
                    border.color: extraInstructionInput.activeFocus
                        ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor

                    TextInput {
                        id: extraInstructionInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        selectByMouse: true
                        clip: true
                        onTextChanged: stagingPage.extraInstructionText = text
                    }

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        verticalAlignment: Text.AlignVCenter
                        visible: extraInstructionInput.text.length === 0
                                 && !extraInstructionInput.activeFocus
                        text: "Anything you want said — \"mention the night shifts\""
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                    }
                }
            }
        }

        // --- What is happening right now ------------------------------
        //
        // The text appearing is the progress indicator. This app does not use
        // spinners.
        Rectangle {
            id: workingPanel
            anchors.top: actionBar.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            height: visible ? Math.min(180, workingColumn.implicitHeight + 24) : 0
            visible: stagingPage.stagingPacketViewModel.isBusy
            radius: 8
            color: JobCrushTheme.cardBackgroundColor
            border.width: 1
            border.color: JobCrushTheme.accentColor
            clip: true

            Column {
                id: workingColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 6

                Text {
                    text: stagingPage.stagingPacketViewModel.busyDescriptionText + "…"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: Font.DemiBold

                    // Fades in and out. Not a spinner.
                    SequentialAnimation on opacity {
                        running: workingPanel.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.45; duration: 900; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0;  duration: 900; easing.type: Easing.InOutQuad }
                    }
                }

                Text {
                    width: parent.width
                    text: stagingPage.stagingPacketViewModel.streamingText
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
            }
        }

        // --- The status line ------------------------------------------
        Rectangle {
            id: noticeBar
            anchors.top: workingPanel.bottom
            anchors.topMargin: stagingPage.stagingPacketViewModel.noticeText.length > 0 ? 12 : 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            height: visible ? noticeColumn.implicitHeight + 20 : 0
            visible: stagingPage.stagingPacketViewModel.noticeText.length > 0
            radius: 8
            color: JobCrushTheme.panelBackgroundColor
            border.width: 1
            border.color: stagingPage.stagingPacketViewModel.noticeIsAProblem
                ? JobCrushTheme.noticeTextColor : JobCrushTheme.positiveColor

            Column {
                id: noticeColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: noticeDismissButton.left
                anchors.margins: 10
                anchors.rightMargin: 8
                spacing: 3

                Text {
                    width: parent.width
                    text: stagingPage.stagingPacketViewModel.noticeText
                    color: stagingPage.stagingPacketViewModel.noticeIsAProblem
                        ? JobCrushTheme.noticeTextColor : JobCrushTheme.primaryTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    visible: stagingPage.stagingPacketViewModel.noticeNextStepText.length > 0
                    text: stagingPage.stagingPacketViewModel.noticeNextStepText
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.WordWrap
                }
            }

            Text {
                id: noticeDismissButton
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 8
                text: "×"
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.titleFontSize

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: stagingPage.stagingPacketViewModel.clearNotice()
                }
            }
        }

        // --- The pieces -----------------------------------------------
        ListView {
            id: packetListView
            anchors.top: noticeBar.bottom
            anchors.topMargin: 14
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: exportBar.top
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.bottomMargin: 12
            clip: true
            spacing: 12
            model: stagingPage.stagingPacketViewModel

            delegate: Rectangle {
                id: pieceCard

                required property int index
                required property string titleText
                required property string markdownText
                required property string kindName
                required property bool wasWrittenByBrain
                required property bool wasEditedByUser
                required property bool isApprovedByUser
                required property bool goesToTheEmployer

                property bool isBeingEdited: false

                width: packetListView.width - 14
                height: pieceColumn.implicitHeight + 28
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.width: pieceCard.isApprovedByUser ? 1 : 2
                border.color: pieceCard.isApprovedByUser
                    ? JobCrushTheme.hairlineBorderColor : JobCrushTheme.noticeTextColor

                Column {
                    id: pieceColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 14
                    spacing: 10

                    Item {
                        width: parent.width
                        height: 24

                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10

                            // This means "I have read this", not "I approve
                            // this".
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20
                                height: 20
                                radius: 5
                                color: pieceCard.isApprovedByUser
                                    ? JobCrushTheme.positiveColor : "transparent"
                                border.width: 2
                                border.color: pieceCard.isApprovedByUser
                                    ? JobCrushTheme.positiveColor
                                    : JobCrushTheme.secondaryTextColor

                                Text {
                                    anchors.centerIn: parent
                                    visible: pieceCard.isApprovedByUser
                                    text: "✓"
                                    color: JobCrushTheme.onAccentTextColor
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: stagingPage.stagingPacketViewModel.setApprovedAt(
                                        pieceCard.index, !pieceCard.isApprovedByUser)
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: pieceCard.titleText.length > 0
                                    ? pieceCard.titleText : pieceCard.kindName
                                color: JobCrushTheme.primaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                                font.weight: Font.DemiBold
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: pieceCard.goesToTheEmployer
                                    ? "goes to the employer" : "yours, stays here"
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: pieceCard.wasEditedByUser
                                text: "· you edited this"
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 14

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: pieceCard.isBeingEdited ? "Done" : "Edit"
                                color: JobCrushTheme.accentColor
                                font.pixelSize: JobCrushTheme.smallFontSize

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (pieceCard.isBeingEdited) {
                                            stagingPage.stagingPacketViewModel.setMarkdownTextAt(
                                                pieceCard.index, pieceEditor.text)
                                        }
                                        pieceCard.isBeingEdited = !pieceCard.isBeingEdited
                                    }
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "×"
                                color: removePieceMouseArea.containsMouse
                                    ? JobCrushTheme.callToActionColor
                                    : JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.titleFontSize

                                MouseArea {
                                    id: removePieceMouseArea
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: stagingPage.stagingPacketViewModel.removePieceAt(
                                        pieceCard.index)
                                }
                            }
                        }
                    }

                    // Rendered view. The user only sees raw markdown if they
                    // click Edit.
                    Text {
                        width: parent.width
                        visible: !pieceCard.isBeingEdited
                        text: pieceCard.markdownText.length > 0
                            ? pieceCard.markdownText
                            : "Empty. Hit Edit and write what you want in here."
                        textFormat: Text.MarkdownText
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }

                    // Edit view: the raw text.
                    Rectangle {
                        width: parent.width
                        visible: pieceCard.isBeingEdited
                        height: visible ? Math.max(140, pieceEditor.implicitHeight + 20) : 0
                        radius: 6
                        color: JobCrushTheme.appBackgroundColor
                        border.width: pieceEditor.activeFocus ? 2 : 1
                        border.color: pieceEditor.activeFocus
                            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor

                        TextEdit {
                            id: pieceEditor
                            anchors.fill: parent
                            anchors.margins: 10
                            text: pieceCard.markdownText
                            color: JobCrushTheme.primaryTextColor
                            font.pixelSize: JobCrushTheme.bodyFontSize
                            selectionColor: JobCrushTheme.accentColor
                            selectedTextColor: JobCrushTheme.onAccentTextColor
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                        }
                    }
                }
            }
        }

        VerticalScrollBar {
            anchors.top: packetListView.top
            anchors.bottom: packetListView.bottom
            anchors.right: parent.right
            anchors.rightMargin: 14
            flickableTarget: packetListView
        }

        // --- Out the door ---------------------------------------------
        Rectangle {
            id: exportBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            // Height follows the Flow, which wraps when the window is
            // narrow. Two rows anchored to opposite edges looked fine at
            // 1400px and overlapped at 1280px, which is the width the window
            // opens at.
            height: exportFlow.implicitHeight + 28
            color: JobCrushTheme.panelBackgroundColor

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: JobCrushTheme.hairlineBorderColor
            }

            Flow {
                id: exportFlow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 10

                // Do not anchor anything inside a Flow. A Flow positions its
                // own children and will not lay out at all if a child anchors
                // itself. Match the chip height instead.
                Text {
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: "Download as"
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                // Two formats. Whichever the user picks becomes the default
                // for next time, so they are not asked on every export.
                Repeater {
                    model: [
                        { formatName: "docx", label: "Word (.docx)" },
                        { formatName: "pdf",  label: "PDF" }
                    ]

                    delegate: Rectangle {
                        id: formatChip

                        required property var modelData

                        readonly property bool isChosen:
                            stagingPage.stagingPacketViewModel.downloadFormat
                            === formatChip.modelData.formatName

                        width: formatChipLabel.implicitWidth + 24
                        height: 32
                        radius: 6
                        color: formatChip.isChosen
                            ? JobCrushTheme.accentColor : JobCrushTheme.cardBackgroundColor
                        border.width: formatChip.isChosen ? 2 : 1
                        border.color: formatChip.isChosen
                            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor

                        Text {
                            id: formatChipLabel
                            anchors.centerIn: parent
                            text: formatChip.modelData.label
                            color: formatChip.isChosen
                                ? JobCrushTheme.onAccentTextColor
                                : JobCrushTheme.primaryTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: stagingPage.stagingPacketViewModel.downloadFormat
                                = formatChip.modelData.formatName
                        }
                    }
                }

                Item { width: 8; height: 1 }

                Rectangle {
                    width: openFolderLabel.implicitWidth + 26
                    height: 38
                    radius: 6
                    color: "transparent"
                    border.width: 1
                    border.color: JobCrushTheme.hairlineBorderColor

                    Text {
                        id: openFolderLabel
                        anchors.centerIn: parent
                        text: "Open the folder"
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: stagingPage.stagingPacketViewModel.openExportFolder()
                    }
                }

                Rectangle {
                    width: exportLabel.implicitWidth + 30
                    height: 38
                    radius: 6
                    color: exportMouseArea.containsMouse
                        ? JobCrushTheme.accentColor : JobCrushTheme.cardBackgroundColor
                    border.width: 1
                    border.color: JobCrushTheme.accentColor

                    Text {
                        id: exportLabel
                        anchors.centerIn: parent
                        text: "Write it out as one file"
                        color: exportMouseArea.containsMouse
                            ? JobCrushTheme.onAccentTextColor : JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                    }

                    MouseArea {
                        id: exportMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: stagingPage.stagingPacketViewModel.exportPacketNow()
                    }
                }

                Rectangle {
                    width: markSentLabel.implicitWidth + 30
                    height: 38
                    radius: 6
                    color: markSentMouseArea.containsMouse
                        ? JobCrushTheme.positiveColor : "transparent"
                    border.width: 1
                    border.color: JobCrushTheme.positiveColor

                    Text {
                        id: markSentLabel
                        anchors.centerIn: parent
                        text: "I've sent it"
                        color: markSentMouseArea.containsMouse
                            ? JobCrushTheme.onAccentTextColor : JobCrushTheme.positiveColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: markSentMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: stagingPage.stagingPacketViewModel.markAsSent()
                    }
                }
            }
        }
    }
}
