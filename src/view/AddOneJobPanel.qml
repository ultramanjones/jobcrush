import QtQuick
import JobCrush

// AddOneJobPanel
//
// Add one job by hand, and let Job Crush go find the real posting.
//
// This is the way around LinkedIn. Job Crush does not read LinkedIn, Indeed or
// Glassdoor — their terms say not to, and an app that gets its user's account
// banned is not helping. But a LinkedIn alert still tells you the company and
// the job title, and most companies that post there also run their own job
// board. So: paste the link, or type the two things off the email, and Job
// Crush goes and finds that same job where it IS allowed to look.
//
// What comes back is the employer's own posting: the whole description, the
// real link, and a job that disappears from the board when it is filled.
//
// Pure view. Everything here goes through the viewmodel.
Rectangle {
    id: addOneJobPanel

    // Injected by the page.
    property var discoveredJobListViewModel

    // Raised when the user asks to be taken to the job that was just added.
    // The panel does not navigate; it does not know where the list is or
    // which tab holds it. It says what the user asked for and the page acts.
    signal showTheJobJustAddedRequested()

    readonly property bool isBusy: discoveredJobListViewModel.leadIsBeingResolved

    color: JobCrushTheme.panelBackgroundColor
    radius: 8
    border.width: 1
    border.color: JobCrushTheme.hairlineBorderColor

    height: panelContent.implicitHeight + 28

    // A text box that matches the ones on the ProDocs page, without dragging
    // in the label and the save-on-leave behavior, neither of which belongs
    // on a box you press a button next to.
    component OneLineBox: Rectangle {
        id: oneLineBox

        property alias typedText: boxInput.text
        property string hintText: ""

        signal returnPressed

        height: 34
        radius: 6
        color: JobCrushTheme.appBackgroundColor
        border.color: boxInput.activeFocus
            ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
        border.width: boxInput.activeFocus ? 2 : 1
        clip: true

        function clear() {
            boxInput.text = ""
        }

        TextInput {
            id: boxInput
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            verticalAlignment: TextInput.AlignVCenter
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
            selectionColor: JobCrushTheme.accentColor
            selectedTextColor: JobCrushTheme.onAccentTextColor
            selectByMouse: true

            Keys.onReturnPressed: oneLineBox.returnPressed()
            Keys.onEnterPressed: oneLineBox.returnPressed()
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            visible: boxInput.text.length === 0 && !boxInput.activeFocus
            text: oneLineBox.hintText
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.bodyFontSize
            elide: Text.ElideRight
        }
    }

    component GoButton: Rectangle {
        id: goButton

        property string labelText: ""
        property bool canBePressed: true

        signal pressed

        // Enter has to come through here, not straight to the signal. Pressing
        // Enter to move from the Company box to the Job title box is a reflex,
        // and going round the disabled check meant that reflex fired a search
        // with half the answer and then cleared both boxes.
        function press() {
            if (canBePressed) {
                pressed()
            }
        }

        width: goButtonLabel.implicitWidth + 28
        height: 34
        radius: 6
        color: canBePressed
            ? (goButtonMouseArea.containsMouse
                   ? Qt.lighter(JobCrushTheme.callToActionColor, 1.15)
                   : JobCrushTheme.callToActionColor)
            : JobCrushTheme.cardBackgroundColor

        Text {
            id: goButtonLabel
            anchors.centerIn: parent
            text: goButton.labelText
            color: goButton.canBePressed
                ? JobCrushTheme.onAccentTextColor : JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: goButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            enabled: goButton.canBePressed
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: goButton.pressed()
        }
    }

    Column {
        id: panelContent
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 14
        spacing: 10

        // --- The link ---
        Item {
            width: parent.width
            height: 34

            Text {
                id: linkRowLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                width: 128
                text: "Job link"
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
            }

            OneLineBox {
                id: linkBox
                anchors.left: linkRowLabel.right
                anchors.leftMargin: 8
                anchors.right: findByLinkButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                hintText: "Greenhouse, Lever or Ashby link"
                onReturnPressed: findByLinkButton.press()
            }

            GoButton {
                id: findByLinkButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                labelText: addOneJobPanel.isBusy ? "looking…" : "Find it"
                canBePressed: !addOneJobPanel.isBusy && linkBox.typedText.trim().length > 0
                onPressed: {
                    addOneJobPanel.discoveredJobListViewModel.addJobFromLink(linkBox.typedText)
                    linkBox.clear()
                }
            }
        }

        // --- Or the two things off an alert email ---
        Item {
            width: parent.width
            height: 34

            Text {
                id: typedRowLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                width: 128
                text: "Or type it"
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
            }

            // The two boxes split whatever is left after the label and the
            // button, half each. Anchored on both sides rather than given a
            // measured width with a floor under it — a floor stops the boxes
            // shrinking but does not stop them meeting, so on a narrow window
            // the old version had them overlapping instead of squeezing.
            OneLineBox {
                id: companyBox
                anchors.left: typedRowLabel.right
                anchors.leftMargin: 8
                anchors.right: parent.horizontalCenter
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                hintText: "Company"
                onReturnPressed: findByNameButton.press()
            }

            OneLineBox {
                id: titleBox
                anchors.left: parent.horizontalCenter
                anchors.leftMargin: 4
                anchors.right: findByNameButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                hintText: "Job title"
                onReturnPressed: findByNameButton.press()
            }

            GoButton {
                id: findByNameButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                labelText: addOneJobPanel.isBusy ? "looking…" : "Go find it"
                canBePressed: !addOneJobPanel.isBusy
                              && companyBox.typedText.trim().length > 0
                              && titleBox.typedText.trim().length > 0
                onPressed: {
                    addOneJobPanel.discoveredJobListViewModel
                        .addJobFromCompanyAndTitle(companyBox.typedText, titleBox.typedText)
                    companyBox.clear()
                    titleBox.clear()
                }
            }
        }

        // What happened, in the app's own words, and the way to go see it.
        //
        // The sentence used to end here, and that was the whole complaint:
        // being told a job is "in Discoveries" is not being told where it is.
        // A list of a thousand rows ranked by score is not somewhere a person
        // can go. So the sentence now comes with the trip.
        Item {
            width: parent.width
            visible: statusSentence.text.length > 0
            implicitHeight: Math.max(statusSentence.implicitHeight,
                                     showMeButton.visible ? showMeButton.height : 0)

            Text {
                id: statusSentence
                anchors.left: parent.left
                anchors.right: showMeButton.visible ? showMeButton.left : parent.right
                anchors.rightMargin: showMeButton.visible ? 10 : 0
                anchors.verticalCenter: parent.verticalCenter
                text: addOneJobPanel.discoveredJobListViewModel.leadStatusText
                color: addOneJobPanel.isBusy
                    ? JobCrushTheme.accentColor : JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.WordWrap
            }

            GoButton {
                id: showMeButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                // Only while there is somewhere to go. A button that lands on
                // nothing is worse than no button.
                visible: !addOneJobPanel.isBusy
                         && addOneJobPanel.discoveredJobListViewModel.hasSomewhereToJumpTo
                labelText: "Show me"
                onPressed: addOneJobPanel.showTheJobJustAddedRequested()
            }
        }

        // The plain truth about which boards this can reach, so nobody thinks
        // it is broken when a company is not on one of them.
        Text {
            width: parent.width
            text: "A LinkedIn link on its own tells Job Crush nothing — type the "
                  + "company and the title off the alert instead. Either way it looks "
                  + "on Greenhouse, Lever and Ashby, and saves what you gave it when "
                  + "the company isn't on one of those."
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            wrapMode: Text.WordWrap
        }
    }
}
