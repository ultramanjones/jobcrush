import QtQuick
import JobCrush

// BrainChatPage
//
// The chat window with AIBrain. Streamed responses appear word by word as
// they are written — which is the progress indicator (no-spinner law: the
// only waiting state is the moment before the first fragment, covered by an
// honest, pulsing status line).
//
// Pure view: binds to BrainChatConversationViewModel, emits navigation
// wishes upward, contains zero business logic.
Rectangle {
    id: brainChatPage

    // Injected from Main: the BrainChatConversationViewModel.
    property var conversationViewModel

    // Injected from Main: the same connection truth Settings shows, so the
    // two screens can never disagree about which brain is live.
    property var brainConnectionViewModel

    // Asks Main to navigate (degraded mode points the user at Settings).
    signal settingsRequested

    color: JobCrushTheme.appBackgroundColor

    // Opening the chat is one of the KEY MOMENTS a connection check exists
    // for — you should never discover a dead brain by typing a paragraph at
    // it first. A cached result answers instantly.
    onVisibleChanged: {
        if (visible) {
            brainConnectionViewModel.checkConnectionNow()
        }
    }
    Component.onCompleted: brainConnectionViewModel.checkConnectionNow()

    // ------------------------------------------------------------------
    // Header
    // ------------------------------------------------------------------
    Item {
        id: chatHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "Moonlight (AI Brain) Chat"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        // The connection truth, echoed from Settings word for word.
        Text {
            id: chatConnectionStatusLabel
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: clearConversationButton.left
            anchors.rightMargin: 20
            text: brainChatPage.brainConnectionViewModel.bannerText
            color: brainChatPage.brainConnectionViewModel.brainIsConnectedAndActive
                ? JobCrushTheme.positiveColor
                : JobCrushTheme.noticeTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8

            // Breathing text while a check is in flight — the honest wait
            // state, and never a spinner.
            SequentialAnimation on opacity {
                running: brainChatPage.brainConnectionViewModel.connectionIsBeingChecked
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.4; duration: 700 }
                NumberAnimation { from: 0.4; to: 1.0; duration: 700 }
                onRunningChanged: {
                    if (!running) {
                        chatConnectionStatusLabel.opacity = 1.0
                    }
                }
            }
        }

        // "New conversation" — quiet text button.
        Rectangle {
            id: clearConversationButton
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 28
            width: clearButtonLabel.implicitWidth + 24
            height: 30
            radius: 6
            color: clearButtonMouseArea.containsMouse
                ? JobCrushTheme.cardBackgroundColor : "transparent"
            border.color: JobCrushTheme.hairlineBorderColor
            border.width: 1

            Text {
                id: clearButtonLabel
                anchors.centerIn: parent
                text: "new conversation"
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
            }

            MouseArea {
                id: clearButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: brainChatPage.conversationViewModel.clearConversation()
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
    // Transcript
    // ------------------------------------------------------------------
    ListView {
        id: transcriptListView

        anchors.top: chatHeader.bottom
        anchors.bottom: composerArea.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        leftMargin: 20
        rightMargin: 20
        topMargin: 16
        bottomMargin: 16
        spacing: 14
        clip: true

        model: brainChatPage.conversationViewModel

        // Keep the newest words on screen while streaming.
        onContentHeightChanged: {
            if (contentHeight > height) {
                positionViewAtEnd()
            }
        }

        delegate: Item {
            id: messageRow

            required property string authorName
            required property string messageText
            required property bool isStillStreaming

            width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
            height: messageBubble.height

            readonly property bool isHumanMessage: authorName === "human"
            readonly property bool isNoticeMessage: authorName === "notice"

            Rectangle {
                id: messageBubble

                // Human messages hug the right edge; Brain and notices sit left.
                anchors.right: messageRow.isHumanMessage ? parent.right : undefined
                anchors.left: messageRow.isHumanMessage ? undefined : parent.left

                width: Math.min(messageContentColumn.implicitWidth + 32,
                                messageRow.width * 0.82)
                height: messageContentColumn.implicitHeight + 24
                radius: 10
                color: messageRow.isNoticeMessage
                    ? "transparent"
                    : (messageRow.isHumanMessage
                           ? JobCrushTheme.humanBubbleColor
                           : JobCrushTheme.brainBubbleColor)
                border.color: messageRow.isNoticeMessage
                    ? JobCrushTheme.hairlineBorderColor : "transparent"
                border.width: messageRow.isNoticeMessage ? 1 : 0

                Column {
                    id: messageContentColumn
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 12
                    width: Math.min(messageTextLabel.implicitWidth,
                                    messageRow.width * 0.82 - 32)
                    spacing: 6

                    Text {
                        id: messageTextLabel
                        width: parent.width
                        // The streaming caret rides at the end of the growing
                        // text — words appearing IS the progress display.
                        text: messageRow.messageText
                              + (messageRow.isStillStreaming && messageRow.messageText.length > 0
                                     ? " ▍" : "")
                        wrapMode: Text.Wrap
                        color: messageRow.isNoticeMessage
                            ? JobCrushTheme.noticeTextColor
                            : JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        visible: messageRow.messageText.length > 0
                    }

                    // The honest waiting line: only alive between "request
                    // sent" and "first word arrived", then gone forever.
                    Text {
                        id: awaitingFirstWordLabel
                        visible: messageRow.isStillStreaming && messageRow.messageText.length === 0
                        text: "Moonlight is reading your message…"
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.italic: true

                        SequentialAnimation on opacity {
                            running: awaitingFirstWordLabel.visible
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.35; duration: 700 }
                            NumberAnimation { from: 0.35; to: 1.0; duration: 700 }
                        }
                    }
                }
            }
        }

        // Empty-transcript welcome (configured brain, nothing said yet).
        Text {
            anchors.centerIn: parent
            visible: transcriptListView.count === 0
                     && brainChatPage.conversationViewModel.brainIsConfigured
            text: "Ask Moonlight anything about your job search."
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
        }
    }

    // ------------------------------------------------------------------
    // Degraded mode: no brain configured — say so, point at Settings.
    // ------------------------------------------------------------------
    Rectangle {
        anchors.centerIn: transcriptListView
        visible: !brainChatPage.conversationViewModel.brainIsConfigured
                 && transcriptListView.count === 0
        width: 380
        height: unconfiguredColumn.implicitHeight + 48
        radius: 12
        color: JobCrushTheme.panelBackgroundColor
        border.color: JobCrushTheme.hairlineBorderColor
        border.width: 1

        Column {
            id: unconfiguredColumn
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 12

            Text {
                width: parent.width
                text: "Moonlight isn't connected yet"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize + 2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: "Job Crush works fine without one — but Moonlight needs "
                      + "an AI provider to speak through. Add an API key in "
                      + "Settings and she wakes right up."
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: openSettingsLabel.implicitWidth + 36
                height: 36
                radius: 8
                color: openSettingsMouseArea.containsMouse
                    ? Qt.lighter(JobCrushTheme.callToActionColor, 1.15)
                    : JobCrushTheme.callToActionColor

                Text {
                    id: openSettingsLabel
                    anchors.centerIn: parent
                    text: "Open Settings"
                    color: JobCrushTheme.onAccentTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: openSettingsMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: brainChatPage.settingsRequested()
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Composer
    // ------------------------------------------------------------------
    Rectangle {
        id: composerArea
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        height: Math.min(Math.max(composerTextEdit.implicitHeight + 24, 52), 160)
        radius: 10
        color: JobCrushTheme.panelBackgroundColor
        border.color: composerTextEdit.activeFocus
            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
        border.width: 1

        Flickable {
            id: composerFlickable
            anchors.fill: parent
            anchors.margins: 12
            anchors.rightMargin: sendButton.width + 24
            contentWidth: width
            contentHeight: composerTextEdit.implicitHeight
            clip: true

            TextEdit {
                id: composerTextEdit
                width: composerFlickable.width
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: TextEdit.Wrap
                selectByMouse: true

                // Enter sends; Shift+Enter makes a newline.
                Keys.onReturnPressed: function(keyEvent) {
                    if (keyEvent.modifiers & Qt.ShiftModifier) {
                        keyEvent.accepted = false
                        return
                    }
                    brainChatPage.sendComposerText()
                    keyEvent.accepted = true
                }
            }

            // Placeholder, hand-rolled (no Controls import needed).
            Text {
                visible: composerTextEdit.text.length === 0 && !composerTextEdit.activeFocus
                text: "Message Moonlight…  (Enter sends, Shift+Enter for a new line)"
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
            }
        }

        Rectangle {
            id: sendButton

            readonly property bool sendIsPossible:
                composerTextEdit.text.trim().length > 0
                && !brainChatPage.conversationViewModel.brainIsResponding

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            width: sendButtonLabel.implicitWidth + 28
            height: 36
            radius: 8
            color: sendIsPossible
                ? (sendButtonMouseArea.containsMouse
                       ? Qt.lighter(JobCrushTheme.accentColor, 1.12)
                       : JobCrushTheme.accentColor)
                : JobCrushTheme.cardBackgroundColor

            Text {
                id: sendButtonLabel
                anchors.centerIn: parent
                text: brainChatPage.conversationViewModel.brainIsResponding
                    ? "answering…" : "Send"
                color: sendButton.sendIsPossible ? JobCrushTheme.onAccentTextColor : JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: sendButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: sendButton.sendIsPossible
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: brainChatPage.sendComposerText()
            }
        }
    }

    // Hands the composer text to the viewmodel and clears the box.
    function sendComposerText() {
        const messageText = composerTextEdit.text.trim()
        if (messageText.length === 0
                || brainChatPage.conversationViewModel.brainIsResponding) {
            return
        }
        brainChatPage.conversationViewModel.sendHumanMessage(messageText)
        composerTextEdit.clear()
    }
}
