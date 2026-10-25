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

    // True while Job Crush is out looking for the posting. The buttons that
    // start a search are off until it is done.
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

        // Marks a box Moonlight left empty because the text did not say.
        property bool needsTheUser: false

        signal returnPressed

        height: 34
        radius: 6
        color: JobCrushTheme.appBackgroundColor
        border.color: boxInput.activeFocus
            ? JobCrushTheme.accentColor
            : (needsTheUser && boxInput.text.length === 0
                   ? JobCrushTheme.noticeTextColor : JobCrushTheme.hairlineBorderColor)
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

            // Tab moves to the next box. A plain TextInput stays out of the
            // Tab order unless it is told to join it, and it was never told.
            activeFocusOnTab: true

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

    // The panel's buttons. Grayed out, and clicks do nothing, while canBePressed is false.
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
                hintText: "Paste the job's link"
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

            // The two boxes split the row at its middle. Each box is anchored
            // on both sides, so on a narrow window both boxes shrink and never
            // overlap. A set width with a minimum would stop them shrinking
            // but would let them run into each other.
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

        // --- Or paste the text: an email, an alert, a posting ---
        //
        // Moonlight reads it and fills in a form. The user checks the form
        // and adds the job. With no brain connected the button is off and the
        // line under it says why and what to do.
        Item {
            width: parent.width
            height: 90

            Text {
                id: pasteRowLabel
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.left: parent.left
                width: 128
                text: "Or paste the text"
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
            }

            Rectangle {
                id: pastedTextFrame
                anchors.left: pasteRowLabel.right
                anchors.leftMargin: 8
                anchors.right: readPastedTextButton.left
                anchors.rightMargin: 8
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                radius: 6
                color: JobCrushTheme.appBackgroundColor
                border.color: pastedTextInput.activeFocus
                    ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
                border.width: pastedTextInput.activeFocus ? 2 : 1
                clip: true

                Flickable {
                    id: pastedTextScroller
                    anchors.fill: parent
                    anchors.margins: 8
                    contentWidth: width
                    contentHeight: pastedTextInput.height
                    clip: true

                    TextEdit {
                        id: pastedTextInput
                        width: pastedTextScroller.width
                        // At least the whole box, so a click anywhere in it
                        // lands in the text.
                        height: Math.max(pastedTextScroller.height, implicitHeight)
                        wrapMode: TextEdit.Wrap
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        selectionColor: JobCrushTheme.accentColor
                        selectedTextColor: JobCrushTheme.onAccentTextColor
                        selectByMouse: true
                        activeFocusOnTab: true
                    }
                }

                Text {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 9
                    visible: pastedTextInput.text.length === 0 && !pastedTextInput.activeFocus
                    text: "Paste a job email, a job alert, a recruiter message, or a "
                          + "posting copied off a page"
                    color: JobCrushTheme.mutedTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }
            }

            GoButton {
                id: readPastedTextButton
                anchors.right: parent.right
                anchors.top: parent.top
                labelText: addOneJobPanel.discoveredJobListViewModel.pastedTextIsBeingRead
                    ? "reading…" : "Have Moonlight fill it in"
                canBePressed: addOneJobPanel.discoveredJobListViewModel.aBrainCanReadPastedText
                              && !addOneJobPanel.discoveredJobListViewModel.pastedTextIsBeingRead
                              && pastedTextInput.text.trim().length > 0
                onPressed: addOneJobPanel.discoveredJobListViewModel
                               .readPastedText(pastedTextInput.text)
            }
        }

        Text {
            width: parent.width
            visible: !addOneJobPanel.discoveredJobListViewModel.aBrainCanReadPastedText
            text: "Connect an AI brain in Settings and Moonlight can read pasted text "
                  + "and fill the job in for you. It costs a few cents of your AI "
                  + "credits each time. Without one, type the company and the title "
                  + "above."
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width
            visible: text.length > 0
            text: addOneJobPanel.discoveredJobListViewModel.pastedTextStatusText
            color: addOneJobPanel.discoveredJobListViewModel.pastedTextIsBeingRead
                ? JobCrushTheme.accentColor : JobCrushTheme.secondaryTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            wrapMode: Text.Wrap
        }

        // --- The form Moonlight filled in ---
        Column {
            id: filledFormColumn
            width: parent.width
            spacing: 6
            visible: addOneJobPanel.discoveredJobListViewModel.filledForm.hasForm === true

            readonly property string emptyHint: "not in the text. Type it if you know it"

            // Fills the boxes each time Moonlight answers. Set by hand, not by
            // binding, so what the user types is never overwritten while
            // they are typing.
            Connections {
                target: addOneJobPanel.discoveredJobListViewModel
                function onPastedTextFormChanged() {
                    const form = addOneJobPanel.discoveredJobListViewModel.filledForm
                    if (form.hasForm !== true) {
                        return
                    }
                    formCompanyBox.typedText = form.companyName
                    formTitleBox.typedText = form.positionTitle
                    formLinkBox.typedText = form.sourceUrl
                    formWhereBox.typedText = form.locationText
                    formPayBox.typedText = form.salaryText
                    formPostedBox.typedText = form.postedText
                    formRemoteToggle.isRemote = form.isRemoteRole
                }
            }

            // One row of the form: a label on the left, and whatever box is
            // placed inside the row fills the rest.
            component FormRow: Item {
                property string labelText: ""
                default property alias boxSlot: boxHolder.data
                width: filledFormColumn.width
                height: 34

                Text {
                    id: formRowLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    width: 128
                    text: parent.labelText
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                Item {
                    id: boxHolder
                    anchors.left: formRowLabel.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                }
            }

            FormRow {
                labelText: "Company"
                OneLineBox { id: formCompanyBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Company " + filledFormColumn.emptyHint }
            }
            FormRow {
                labelText: "Job title"
                OneLineBox { id: formTitleBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Job title " + filledFormColumn.emptyHint }
            }
            FormRow {
                labelText: "Link"
                OneLineBox { id: formLinkBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Link " + filledFormColumn.emptyHint }
            }
            FormRow {
                labelText: "Where"
                OneLineBox { id: formWhereBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Place " + filledFormColumn.emptyHint }
            }
            FormRow {
                labelText: "Pay"
                OneLineBox { id: formPayBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Pay " + filledFormColumn.emptyHint }
            }
            FormRow {
                labelText: "Posted on"
                OneLineBox { id: formPostedBox; anchors.fill: parent; needsTheUser: true
                             hintText: "Date " + filledFormColumn.emptyHint }
            }

            // The Remote tick box and the two buttons. Pushed right by the
            // 128-pixel label plus its 8-pixel gap, so the row lines up
            // with the boxes above.
            Row {
                spacing: 10
                x: 136

                Rectangle {
                    id: formRemoteToggle
                    property bool isRemote: false
                    width: 22
                    height: 22
                    radius: 5
                    anchors.verticalCenter: parent.verticalCenter
                    color: isRemote ? JobCrushTheme.positiveColor : "transparent"
                    border.width: 2
                    border.color: isRemote ? JobCrushTheme.positiveColor
                                           : JobCrushTheme.secondaryTextColor

                    Text {
                        anchors.centerIn: parent
                        visible: formRemoteToggle.isRemote
                        text: "\u2713"
                        color: JobCrushTheme.onAccentTextColor
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: formRemoteToggle.isRemote = !formRemoteToggle.isRemote
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Remote"
                    color: JobCrushTheme.primaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                GoButton {
                    labelText: addOneJobPanel.isBusy ? "looking…" : "Add this job"
                    canBePressed: !addOneJobPanel.isBusy
                                  && formCompanyBox.typedText.trim().length > 0
                                  && formTitleBox.typedText.trim().length > 0
                    onPressed: {
                        addOneJobPanel.discoveredJobListViewModel.addJobFromFilledForm(
                            formCompanyBox.typedText, formTitleBox.typedText,
                            formLinkBox.typedText, formWhereBox.typedText,
                            formPayBox.typedText, formPostedBox.typedText,
                            formRemoteToggle.isRemote)
                        pastedTextInput.text = ""
                    }
                }

                GoButton {
                    labelText: "Clear"
                    onPressed: {
                        addOneJobPanel.discoveredJobListViewModel.clearFilledForm()
                        pastedTextInput.text = ""
                    }
                }
            }
        }

        // What happened, in the app's own words, and a button to go see it.
        //
        // Being told a job is "in Discoveries" does not tell the user where
        // it is. The list can hold a thousand rows ranked by score. So the
        // sentence comes with a Show me button that goes to the row.
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
                // Shown only when there is a job to go to. A button that
                // goes nowhere would only confuse the user.
                visible: !addOneJobPanel.isBusy
                         && addOneJobPanel.discoveredJobListViewModel.hasSomewhereToJumpTo
                labelText: "Show me"
                onPressed: addOneJobPanel.showTheJobJustAddedRequested()
            }
        }

        // Which boards this can reach. Said up front, so nobody thinks it is
        // broken when a company is not on one of them.
        Text {
            width: parent.width
            text: "Paste the job's own page and Job Crush reads the job off it. "
                  + "A LinkedIn or Indeed link tells it nothing — type the company "
                  + "and the title off the alert instead. Either way it checks "
                  + "Greenhouse, Lever, Ashby and SmartRecruiters, then the page, then asks your AI "
                  + "brain to search the web if one is connected. What you gave it "
                  + "is saved even when nothing is found."
            color: JobCrushTheme.mutedTextColor
            font.pixelSize: JobCrushTheme.smallFontSize
            wrapMode: Text.WordWrap
        }
    }
}
