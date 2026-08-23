import QtQuick

// InlineEditField
//
// A labelled text box that saves what you typed the moment you leave it.
//
// Used everywhere the user corrects something Job Crush read out of their
// documents. No Save button anywhere near it — settings and corrections are
// instant-apply by law, and a Save button on a row of a list is exactly the
// ceremony that makes people stop bothering to fix things.
//
// Set allowsMultipleLines and the box GROWS to fit what is in it, as it is
// typed or pasted. A one-line box holding a paragraph is the app telling
// somebody their own work history is too long to look at — they cannot check
// what they cannot see, and checking it is the entire job of that screen.
Item {
    id: inlineEditField

    property string labelText: ""
    property string fieldText: ""
    property string placeholderText: ""

    // Grow to fit, wrap, and accept Return as a new line rather than as
    // "done". Off by default: a job title is one line and should stay one.
    property bool allowsMultipleLines: false

    // Where growing stops and scrolling starts. Generous on purpose — a
    // twelve-line description of a job should simply be twelve lines.
    property int tallestBeforeItScrolls: 420

    readonly property int shortestFrameHeight: 32

    // Emitted when the user has finished with the box and the value changed.
    signal editingCommitted(string newText)

    implicitHeight: fieldLabel.height + fieldFrame.height + 4

    Text {
        id: fieldLabel
        anchors.top: parent.top
        anchors.left: parent.left
        text: inlineEditField.labelText
        color: JobCrushTheme.mutedTextColor
        font.pixelSize: JobCrushTheme.smallFontSize
    }

    Rectangle {
        id: fieldFrame
        anchors.top: fieldLabel.bottom
        anchors.topMargin: 3
        anchors.left: parent.left
        anchors.right: parent.right

        // The whole point: the frame is as tall as the words inside it, until
        // it hits the ceiling and hands the rest over to scrolling.
        height: inlineEditField.allowsMultipleLines
            ? Math.min(inlineEditField.tallestBeforeItScrolls,
                       Math.max(inlineEditField.shortestFrameHeight,
                                fieldInput.implicitHeight + 14))
            : inlineEditField.shortestFrameHeight

        radius: 6
        color: JobCrushTheme.appBackgroundColor
        border.color: fieldInput.activeFocus
            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
        border.width: fieldInput.activeFocus ? 2 : 1
        clip: true

        // Only ever scrolls when the text has outgrown the ceiling above.
        Flickable {
            id: fieldScroller
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            anchors.topMargin: inlineEditField.allowsMultipleLines ? 7 : 0
            anchors.bottomMargin: inlineEditField.allowsMultipleLines ? 7 : 0
            interactive: inlineEditField.allowsMultipleLines
            contentWidth: width
            contentHeight: fieldInput.implicitHeight
            clip: true

            // Follows the cursor, so typing at the bottom of a long entry
            // never disappears under the edge of the frame.
            function keepCursorInView() {
                const cursor = fieldInput.cursorRectangle
                if (cursor.y < contentY) {
                    contentY = cursor.y
                } else if (cursor.y + cursor.height > contentY + height) {
                    contentY = cursor.y + cursor.height - height
                }
            }

            TextEdit {
                id: fieldInput

                width: fieldScroller.width
                height: Math.max(implicitHeight, fieldScroller.height)
                verticalAlignment: inlineEditField.allowsMultipleLines
                    ? TextEdit.AlignTop : TextEdit.AlignVCenter
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                selectionColor: JobCrushTheme.accentColor
                selectedTextColor: JobCrushTheme.onAccentTextColor
                selectByMouse: true

                // A single-line field must not wrap, or it grows a second line
                // the moment somebody pastes a long job title into it.
                wrapMode: inlineEditField.allowsMultipleLines
                    ? TextEdit.Wrap : TextEdit.NoWrap

                // Bound to the viewmodel, so a re-read or a correction made
                // elsewhere shows up here without this field having to be told.
                text: inlineEditField.fieldText

                onCursorRectangleChanged: fieldScroller.keepCursorInView()

                function commitIfChanged() {
                    if (text !== inlineEditField.fieldText) {
                        inlineEditField.editingCommitted(text)
                    }
                }

                // On a one-line field Return means "I'm done". On a grown one
                // it means what it says on the key.
                Keys.onReturnPressed: function(keyEvent) {
                    if (inlineEditField.allowsMultipleLines) {
                        keyEvent.accepted = false
                        return
                    }
                    fieldInput.commitIfChanged()
                    fieldInput.focus = false
                }
                Keys.onEnterPressed: function(keyEvent) {
                    if (inlineEditField.allowsMultipleLines) {
                        keyEvent.accepted = false
                        return
                    }
                    fieldInput.commitIfChanged()
                    fieldInput.focus = false
                }

                onActiveFocusChanged: {
                    if (!activeFocus) {
                        commitIfChanged()
                    }
                }
            }
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: inlineEditField.allowsMultipleLines ? 8 : 0
            anchors.left: parent.left
            anchors.leftMargin: 10
            height: inlineEditField.allowsMultipleLines
                ? implicitHeight : parent.height
            verticalAlignment: inlineEditField.allowsMultipleLines
                ? Text.AlignTop : Text.AlignVCenter
            visible: fieldInput.text.length === 0 && !fieldInput.activeFocus
            text: inlineEditField.placeholderText
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
        }
    }
}
