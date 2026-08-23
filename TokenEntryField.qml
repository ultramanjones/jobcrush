import QtQuick

// TokenEntryField
//
// A text field whose entries become removable chips. Built because a
// comma-separated box cannot hold "Pittsburgh, PA" — the separator is inside
// the value. Chips solve that outright: each entry is its own object, so a
// comma in one means nothing to the next.
//
// Behaviour, aimed at costing the user as little attention as possible:
//   type            → suggestions drop down beneath the field
//   Up / Down       → move through them
//   Enter / Tab     → take the highlighted suggestion, or what was typed
//   click           → take that suggestion
//   Backspace empty → remove the last chip
//   × on a chip     → remove that one
//
// Anything typed is accepted whether or not it was suggested. Suggestions are
// a convenience, never a gate.
//
// Pure view: it holds the tokens it was given and announces what the user
// wants done. Adding, removing and suggesting all happen above it.
Item {
    id: tokenEntryField

    // The chips to show, in order.
    property var tokens: []

    // Called with what has been typed; returns an array of suggestions.
    // Supplied by whoever uses this field, so this component stays ignorant
    // of what it is collecting.
    property var suggestionProvider: null

    property string placeholderText: ""

    // The user wants this added / the chip at this index gone.
    signal tokenAdded(string tokenText)
    signal tokenRemovedAt(int tokenIndex)

    implicitHeight: tokenFieldFrame.height

    // What the suggestion list is showing right now, and which row is lit.
    property var currentSuggestions: []
    property int highlightedSuggestionIndex: -1

    readonly property bool suggestionsAreShowing:
        currentSuggestions.length > 0 && tokenTextInput.activeFocus

    function refreshSuggestions() {
        if (suggestionProvider === null || tokenTextInput.text.trim().length === 0) {
            currentSuggestions = []
            highlightedSuggestionIndex = -1
            return
        }
        currentSuggestions = suggestionProvider(tokenTextInput.text)
        highlightedSuggestionIndex = currentSuggestions.length > 0 ? 0 : -1
    }

    // Takes the highlighted suggestion if there is one, otherwise whatever
    // was typed. Someone who ignores the dropdown entirely still gets exactly
    // what they asked for.
    function commitCurrentEntry() {
        let textToAdd = tokenTextInput.text.trim()
        if (highlightedSuggestionIndex >= 0
                && highlightedSuggestionIndex < currentSuggestions.length) {
            textToAdd = currentSuggestions[highlightedSuggestionIndex]
        }
        if (textToAdd.length === 0) {
            return
        }
        tokenEntryField.tokenAdded(textToAdd)
        tokenTextInput.text = ""
        currentSuggestions = []
        highlightedSuggestionIndex = -1
    }

    Rectangle {
        id: tokenFieldFrame

        width: parent.width
        height: Math.max(tokenFlow.implicitHeight + 16, 40)
        radius: 8
        color: JobCrushTheme.appBackgroundColor
        border.color: tokenTextInput.activeFocus
            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
        border.width: tokenTextInput.activeFocus ? 2 : 1

        Flow {
            id: tokenFlow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 6

            Repeater {
                model: tokenEntryField.tokens

                delegate: Rectangle {
                    id: tokenChip

                    required property int index
                    required property string modelData

                    width: tokenChipLabel.implicitWidth + tokenChipRemoveLabel.implicitWidth + 22
                    height: 24
                    radius: 12
                    color: JobCrushTheme.cardBackgroundColor
                    border.color: JobCrushTheme.accentColor
                    border.width: 1

                    Text {
                        id: tokenChipLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: tokenChip.modelData
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                    }

                    Text {
                        id: tokenChipRemoveLabel
                        anchors.left: tokenChipLabel.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×"
                        color: tokenChipRemoveMouseArea.containsMouse
                            ? JobCrushTheme.callToActionColor : JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize

                        MouseArea {
                            id: tokenChipRemoveMouseArea
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tokenEntryField.tokenRemovedAt(tokenChip.index)
                        }
                    }
                }
            }

            TextInput {
                id: tokenTextInput

                width: Math.max(140, tokenFlow.width - 8)
                height: 24
                verticalAlignment: TextInput.AlignVCenter
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                clip: true

                onTextChanged: tokenEntryField.refreshSuggestions()

                Keys.onDownPressed: {
                    if (tokenEntryField.currentSuggestions.length > 0) {
                        tokenEntryField.highlightedSuggestionIndex =
                            (tokenEntryField.highlightedSuggestionIndex + 1)
                            % tokenEntryField.currentSuggestions.length
                    }
                }

                Keys.onUpPressed: {
                    if (tokenEntryField.currentSuggestions.length > 0) {
                        tokenEntryField.highlightedSuggestionIndex =
                            (tokenEntryField.highlightedSuggestionIndex
                             + tokenEntryField.currentSuggestions.length - 1)
                            % tokenEntryField.currentSuggestions.length
                    }
                }

                Keys.onReturnPressed: tokenEntryField.commitCurrentEntry()
                Keys.onEnterPressed: tokenEntryField.commitCurrentEntry()
                Keys.onTabPressed: tokenEntryField.commitCurrentEntry()

                Keys.onEscapePressed: {
                    tokenEntryField.currentSuggestions = []
                    tokenEntryField.highlightedSuggestionIndex = -1
                }

                // Backspace in an empty box takes back the last chip — the
                // shortcut anyone who has used a chip field already expects.
                Keys.onPressed: function(keyEvent) {
                    if (keyEvent.key === Qt.Key_Backspace
                            && tokenTextInput.text.length === 0
                            && tokenEntryField.tokens.length > 0) {
                        tokenEntryField.tokenRemovedAt(tokenEntryField.tokens.length - 1)
                        keyEvent.accepted = true
                    }
                }

                // Deliberately NOT auto-committing when focus leaves. A click
                // on a suggestion also moves focus, and racing those two
                // would sometimes add the typed text instead of the thing the
                // user just clicked — the exact kind of "it did something
                // else" moment that makes people distrust an app. Typed text
                // simply stays in the box, and the hint below says what to do
                // with it.
            }
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: tokenEntryField.tokens.length === 0
                     && tokenTextInput.text.length === 0
                     && !tokenTextInput.activeFocus
            text: tokenEntryField.placeholderText
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
        }

        MouseArea {
            anchors.fill: parent
            // Clicking anywhere in the field puts the cursor in the box, the
            // way every chip field the user has ever met behaves.
            z: -1
            onClicked: tokenTextInput.forceActiveFocus()
        }
    }

    // Says what to do with text that has been typed but not turned into a
    // chip yet, so nobody wanders off assuming it was added.
    Text {
        anchors.top: tokenFieldFrame.bottom
        anchors.topMargin: 4
        anchors.left: tokenFieldFrame.left
        visible: !tokenEntryField.suggestionsAreShowing
                 && tokenTextInput.text.trim().length > 0
        text: "press Enter to add \u201C" + tokenTextInput.text.trim() + "\u201D"
        color: JobCrushTheme.noticeTextColor
        font.pixelSize: JobCrushTheme.smallFontSize
    }

    // ------------------------------------------------------------------
    // The suggestion drop-down
    // ------------------------------------------------------------------
    Rectangle {
        id: suggestionDropDown

        anchors.top: tokenFieldFrame.bottom
        anchors.topMargin: 4
        anchors.left: tokenFieldFrame.left
        width: Math.min(320, tokenFieldFrame.width)
        height: suggestionColumn.implicitHeight + 8
        visible: tokenEntryField.suggestionsAreShowing
        radius: 8
        color: JobCrushTheme.panelBackgroundColor
        border.color: JobCrushTheme.accentColor
        border.width: 1
        // Above whatever follows it in the settings column.
        z: 100

        Column {
            id: suggestionColumn
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 4

            Repeater {
                model: tokenEntryField.currentSuggestions

                delegate: Rectangle {
                    id: suggestionRow

                    required property int index
                    required property string modelData

                    width: suggestionColumn.width
                    height: 28
                    radius: 5
                    color: tokenEntryField.highlightedSuggestionIndex === suggestionRow.index
                        ? JobCrushTheme.cardBackgroundColor : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: suggestionRow.modelData
                        color: tokenEntryField.highlightedSuggestionIndex === suggestionRow.index
                            ? JobCrushTheme.primaryTextColor : JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: tokenEntryField.highlightedSuggestionIndex = suggestionRow.index
                        onClicked: {
                            tokenEntryField.highlightedSuggestionIndex = suggestionRow.index
                            tokenEntryField.commitCurrentEntry()
                        }
                    }
                }
            }
        }
    }
}
