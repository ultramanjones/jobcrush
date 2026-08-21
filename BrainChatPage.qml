import QtQuick

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

    // Asks Main to navigate (degraded mode points the user at Settings).
    signal settingsRequested

    color: JobCrushTheme.appBackgroundColor

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
            text: "Brain Chat"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: clearConversationButton.left
            anchors.rightMargin: 20
            visible: brainChatPage.conversationViewModel.brainIsConfigured
            text: "speaking with " + brainChatPage.conversationViewModel.activeProviderName
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
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
                        text: "AIBrain is reading your message…"
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
            text: "Ask AIBrain anything about your job search."
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
                text: "No brain configured yet"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize + 2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: "Job Crush works fine without one — but Brain Chat needs "
                      + "an AI provider. Add an API key in Settings to wake it up."
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
                    color: "#0D0F14"
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
                text: "Message AIBrain…  (Enter sends, Shift+Enter for a new line)"
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
                color: sendButton.sendIsPossible ? "#0D0F14" : JobCrushTheme.mutedTextColor
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
