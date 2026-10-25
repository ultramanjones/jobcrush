import QtQuick
import JobCrush

// BrainChatPage
//
// The chat window with Moonlight, the app's AI brain. Answers show up word
// by word as they are written. The growing text is the progress display, so
// the page needs no spinner. The only wait with no words yet is before the
// first word arrives. A pulsing line covers that wait.
//
// This is the view layer. It binds to BrainChatConversationViewModel, asks
// Main to change pages, and holds no business logic.
Rectangle {
    id: brainChatPage

    // Injected from Main: the BrainChatConversationViewModel.
    property var conversationViewModel

    // Injected from Main: the same connection viewmodel Settings uses, so
    // the two screens always agree about which brain is connected.
    property var brainConnectionViewModel

    // Injected from Main: the hints, and whether the strip was closed.
    property var preferencesViewModel

    // Asks Main to show Settings. Used when no brain is set up yet.
    signal settingsRequested

    color: JobCrushTheme.appBackgroundColor

    // Check the connection each time the chat opens. The user should not
    // type a long message and only then find out the brain is not working.
    // A recent result is reused, so this answers at once.
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

        // The connection status, in the same words Settings shows.
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

            // The text fades in and out while a check is running. This is
            // the waiting state instead of a spinner. When the check ends,
            // the text goes back to full strength.
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
        anchors.bottom: hintsStrip.visible ? hintsStrip.top : composerArea.top
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

            // The viewmodel names who wrote each message: "human" for the
            // user, "notice" for a note from the app, and anything else is
            // Moonlight.
            readonly property bool isHumanMessage: authorName === "human"
            readonly property bool isNoticeMessage: authorName === "notice"

            // A bubble never takes more than this share of the row, so the
            // left or right side still shows who is talking.
            readonly property real widestBubbleShare: 0.82

            // The space inside a bubble that is not text: the side margins
            // and a little more.
            readonly property int bubblePaddingWidth: 32

            Rectangle {
                id: messageBubble

                // Human messages hug the right edge; Brain and notices sit left.
                anchors.right: messageRow.isHumanMessage ? parent.right : undefined
                anchors.left: messageRow.isHumanMessage ? undefined : parent.left

                width: Math.min(messageContentColumn.implicitWidth + messageRow.bubblePaddingWidth,
                                messageRow.width * messageRow.widestBubbleShare)
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
                                    messageRow.width * messageRow.widestBubbleShare
                                        - messageRow.bubblePaddingWidth)
                    spacing: 6

                    Text {
                        id: messageTextLabel
                        width: parent.width
                        // While the answer is still coming, a bar sits at the
                        // end of the text. The words showing up are the
                        // progress display.
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

                    // The waiting line. Shown only after the message is sent
                    // and before the first word of the answer arrives.
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
    // No brain set up yet. Say so, and offer a button to Settings.
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
    // Hints: one thing Job Crush can do that the user may not have found.
    // A new one every 20 seconds. "hide tips" closes the strip for good.
    // ------------------------------------------------------------------
    Item {
        id: hintsStrip
        anchors.bottom: composerArea.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: 26
        visible: brainChatPage.preferencesViewModel !== undefined
                 && brainChatPage.preferencesViewModel !== null
                 && !brainChatPage.preferencesViewModel.moonlightHintsAreDismissed
                 && brainChatPage.preferencesViewModel.moonlightTipsAreTurnedOn
                 && brainChatPage.preferencesViewModel.moonlightHints.length > 0

        // Which hint is showing. It steps through the list and goes back to
        // the first one after the last.
        property int hintIndex: 0

        readonly property int millisecondsBetweenHints: 20000

        // Runs only while the strip and the page are both showing, so the
        // hints do not move on while nobody can see them.
        Timer {
            interval: hintsStrip.millisecondsBetweenHints
            repeat: true
            running: hintsStrip.visible && brainChatPage.visible
            onTriggered: hintsStrip.hintIndex = (hintsStrip.hintIndex + 1)
                         % brainChatPage.preferencesViewModel.moonlightHints.length
        }

        Text {
            anchors.left: parent.left
            anchors.right: closeHintsLabel.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: hintsStrip.visible
                ? "Tip: " + brainChatPage.preferencesViewModel.moonlightHints[hintsStrip.hintIndex]
                : ""
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            elide: Text.ElideRight
        }

        Text {
            id: closeHintsLabel
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "hide tips"
            color: closeHintsMouseArea.containsMouse
                ? JobCrushTheme.accentColor : JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.underline: closeHintsMouseArea.containsMouse

            MouseArea {
                id: closeHintsMouseArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: brainChatPage.preferencesViewModel.moonlightHintsAreDismissed = true
            }
        }
    }

    // ------------------------------------------------------------------
    // Composer: the box the user types in, and the Send button
    // ------------------------------------------------------------------
    Rectangle {
        id: composerArea

        // The box grows with the text, from one line up to a few lines.
        // Past that, the text scrolls inside the box.
        readonly property int shortestComposerHeight: 52
        readonly property int tallestComposerHeight: 160

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        height: Math.min(Math.max(composerTextEdit.implicitHeight + 24, shortestComposerHeight),
                         tallestComposerHeight)
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

            // Hint text for the empty box. Drawn here by hand, so the page
            // does not need the Qt Quick Controls module.
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

    // Hands the composer text to the viewmodel and clears the box. Does
    // nothing when the box is empty or Moonlight is still answering. Both
    // Enter and the Send button come through here.
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
