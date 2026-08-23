import QtQuick

// InlineEditField
//
// A labelled text box that saves what you typed the moment you leave it.
//
// Used everywhere the user corrects something Job Crush read out of their
// documents. No Save button anywhere near it — settings and corrections are
// instant-apply by law, and a Save button on a row of a list is exactly the
// ceremony that makes people stop bothering to fix things.
Item {
    id: inlineEditField

    property string labelText: ""
    property string fieldText: ""
    property string placeholderText: ""

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
        height: 32
        radius: 6
        color: JobCrushTheme.appBackgroundColor
        border.color: fieldInput.activeFocus
            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
        border.width: fieldInput.activeFocus ? 2 : 1

        TextInput {
            id: fieldInput
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            verticalAlignment: TextInput.AlignVCenter
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
            clip: true
            selectByMouse: true

            // Bound to the viewmodel, so a re-read or a correction made
            // elsewhere shows up here without this field having to be told.
            text: inlineEditField.fieldText

            function commitIfChanged() {
                if (text !== inlineEditField.fieldText) {
                    inlineEditField.editingCommitted(text)
                }
            }

            onEditingFinished: commitIfChanged()
            onActiveFocusChanged: {
                if (!activeFocus) {
                    commitIfChanged()
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            visible: fieldInput.text.length === 0 && !fieldInput.activeFocus
            text: inlineEditField.placeholderText
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
        }
    }
}
