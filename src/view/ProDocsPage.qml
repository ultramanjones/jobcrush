import QtQuick
import JobCrush

// ProDocsPage
//
// Professional Documentation — everything the user has handed Job Crush, and
// what it understood from it. Two tabs: the documents themselves, and the
// experience and education pulled out of them.
//
// Files are dropped in to be read here, but the drop area is not on this
// page. It is on the window (see Main.qml), so a drop works from any screen.
// If it were here, a resume dropped on the chat page would do nothing, and
// the user would think the feature was broken.
//
// This is the view layer. It binds to three viewmodels and holds no
// business logic.
Rectangle {
    id: proDocsPage

    // Injected from Main.
    property ProfessionalDocumentListViewModel professionalDocumentListViewModel
    property WorkExperienceListViewModel workExperienceListViewModel
    property EducationListViewModel educationListViewModel

    color: JobCrushTheme.appBackgroundColor

    // "documents" | "experience"
    property string activeTabName: "documents"

    // Sent to every experience and schooling card at once.
    //
    // A signal, not a binding, on purpose. A card the user opens or shuts by
    // clicking must STAY that way. A binding would put it back the next time
    // the bound value changed. With a signal, each card sets itself once
    // when the signal comes, and after that only the user's clicks change it.
    signal setEveryCareerCardExpanded(bool shouldExpand)

    Item {
        id: proDocsHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 28
            text: "ProDocs"
            color: JobCrushTheme.primaryTextColor
            font.pixelSize: JobCrushTheme.titleFontSize
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 130
            text: "Professional Documentation"
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

    Row {
        id: proDocsTabRow
        anchors.top: proDocsHeader.bottom
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.topMargin: 14
        spacing: 10

        Repeater {
            model: [
                { tabName: "documents",  displayLabel: "Documents" },
                { tabName: "experience", displayLabel: "Experience & Education" }
            ]

            delegate: Rectangle {
                id: proDocsTab

                required property var modelData

                readonly property bool isActiveTab: proDocsPage.activeTabName === modelData.tabName

                width: proDocsTabLabel.implicitWidth + 28
                height: 32
                radius: 16
                color: isActiveTab ? JobCrushTheme.cardBackgroundColor : "transparent"
                border.color: isActiveTab
                    ? JobCrushTheme.accentColor : JobCrushTheme.hairlineBorderColor
                border.width: isActiveTab ? 2 : 1

                Text {
                    id: proDocsTabLabel
                    anchors.centerIn: parent
                    text: proDocsTab.modelData.displayLabel
                    color: proDocsTab.isActiveTab
                        ? JobCrushTheme.primaryTextColor : JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.weight: proDocsTab.isActiveTab ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: proDocsPage.activeTabName = proDocsTab.modelData.tabName
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Documents
    // ------------------------------------------------------------------
    ListView {
        id: documentListView

        visible: proDocsPage.activeTabName === "documents"
        anchors.top: proDocsTabRow.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 28
        anchors.rightMargin: 46
        anchors.bottomMargin: 20
        spacing: 6
        clip: true

        model: proDocsPage.professionalDocumentListViewModel

        // One notch of the mouse wheel scrolls twice as far as Qt's default.
        //
        // The steps: turn the wheel's raw count into notches, turn notches
        // into pixels, move the list by that much, and keep the result
        // between the top and the bottom of the list. A list that fits does
        // not scroll.
        WheelHandler {
            // Qt reports one wheel notch as 120 units of angleDelta.
            readonly property real angleDeltaPerNotch: 120
            readonly property real pixelsPerNotch: 120
            readonly property real speedMultiplier: 2.0
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(wheelEvent) {
                const scrollableDistance = Math.max(
                    0, documentListView.contentHeight - documentListView.height)
                if (scrollableDistance <= 0) {
                    return
                }
                documentListView.contentY = Math.max(
                    documentListView.originY,
                    Math.min(documentListView.originY + scrollableDistance,
                             documentListView.contentY
                             - (wheelEvent.angleDelta.y / angleDeltaPerNotch)
                               * pixelsPerNotch * speedMultiplier))
            }
        }

        delegate: Rectangle {
            id: documentRow

            required property int index
            required property string displayName
            required property string documentKind
            required property string documentKindLabel
            required property string importedWhenText
            required property string fileSizeText
            required property string extractionNote
            required property bool hasReadableText
            required property string wordCountText

            width: ListView.view.width
            height: documentRowColumn.implicitHeight + 26
            radius: 8
            color: documentRowMouseArea.containsMouse
                ? JobCrushTheme.cardBackgroundColor : JobCrushTheme.panelBackgroundColor
            border.color: JobCrushTheme.hairlineBorderColor
            border.width: 1

            // What kind of document Job Crush thinks this is. The guess is
            // sometimes wrong, so one click moves it to the next kind. The
            // user clicks until it is right.
            Rectangle {
                id: documentKindChip
                anchors.right: removeDocumentLabel.left
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 14
                width: documentKindChipLabel.implicitWidth + 20
                height: 24
                radius: 12
                color: "transparent"
                border.width: 1
                border.color: documentRow.documentKind === "other"
                    ? JobCrushTheme.hairlineBorderColor : JobCrushTheme.accentColor

                Text {
                    id: documentKindChipLabel
                    anchors.centerIn: parent
                    text: documentRow.documentKindLabel
                    color: documentRow.documentKind === "other"
                        ? JobCrushTheme.mutedTextColor : JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    // The steps: get the list of kinds, find this document's
                    // kind in it, and take the one after it. After the last
                    // kind comes the first. A kind that is not in the list
                    // gives -1, so the click picks the first kind.
                    onClicked: {
                        const selectableKinds = proDocsPage.professionalDocumentListViewModel
                            .selectableDocumentKinds()
                        const nextIndex =
                            (selectableKinds.indexOf(documentRow.documentKind) + 1)
                            % selectableKinds.length
                        proDocsPage.professionalDocumentListViewModel
                            .setDocumentKindAt(documentRow.index, selectableKinds[nextIndex])
                    }
                }
            }

            Text {
                id: removeDocumentLabel
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 13
                text: "×"
                color: removeDocumentMouseArea.containsMouse
                    ? JobCrushTheme.callToActionColor : JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.titleFontSize

                MouseArea {
                    id: removeDocumentMouseArea
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: proDocsPage.professionalDocumentListViewModel
                        .removeDocumentAt(documentRow.index)
                }
            }

            Column {
                id: documentRowColumn
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: documentKindChip.left
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 13
                spacing: 4

                Text {
                    width: parent.width
                    text: documentRow.displayName
                    color: JobCrushTheme.primaryTextColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }

                Text {
                    width: parent.width
                    text: documentRow.importedWhenText
                          + (documentRow.fileSizeText.length > 0
                                 ? "   ·   " + documentRow.fileSizeText : "")
                          + "   ·   " + documentRow.wordCountText
                    // The word count shows whether the text was read. The
                    // line turns amber when no readable text came out.
                    color: documentRow.hasReadableText
                        ? JobCrushTheme.secondaryTextColor : JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    visible: documentRow.extractionNote.length > 0
                    text: documentRow.extractionNote
                    color: JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }

                // "Moonlight, clean this up!" Hands the document to the
                // connected brain to read the jobs and schools again. With no
                // brain the button is off and the line says why.
                Column {
                    width: parent.width
                    visible: documentRow.hasReadableText
                    spacing: 4

                    Text {
                        width: parent.width
                        text: proDocsPage.professionalDocumentListViewModel.brainCanCleanUp
                            ? "If Job Crush read the jobs or schools in this wrong, you "
                              + "can have Moonlight read it again. It uses a few cents "
                              + "of your AI credits."
                            : "If you have connected an AI brain you can have Moonlight "
                              + "read this again when the jobs or schools came out "
                              + "wrong. Connect one in Settings."
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        readonly property bool canBePressed:
                            proDocsPage.professionalDocumentListViewModel.brainCanCleanUp
                            && !proDocsPage.professionalDocumentListViewModel.cleanupIsRunning

                        width: cleanUpLabel.implicitWidth + 20
                        height: 24
                        radius: 12
                        opacity: canBePressed ? 1.0 : 0.45
                        color: cleanUpMouseArea.containsMouse && canBePressed
                            ? JobCrushTheme.accentColor : "transparent"
                        border.width: 1
                        border.color: JobCrushTheme.accentColor

                        Text {
                            id: cleanUpLabel
                            anchors.centerIn: parent
                            text: proDocsPage.professionalDocumentListViewModel.cleanupIsRunning
                                ? "Moonlight is reading…" : "Moonlight, clean this up!"
                            color: cleanUpMouseArea.containsMouse && parent.canBePressed
                                ? JobCrushTheme.onAccentTextColor : JobCrushTheme.accentColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                        }

                        MouseArea {
                            id: cleanUpMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: parent.canBePressed
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: proDocsPage.professionalDocumentListViewModel
                                           .cleanUpDocumentAt(documentRow.index)
                        }
                    }
                }

                // An employer may ask for a Word file when the user only has
                // a PDF. Job Crush has already read this document and can
                // write those formats, so it converts it here. The user does
                // not have to find a converter on the web.
                //
                // Only offered when there are words to convert. A scan has
                // none, and a button that cannot work would only confuse the
                // user.
                Item {
                    width: parent.width
                    height: documentRow.hasReadableText ? saveCopyColumn.implicitHeight + 8 : 0
                    visible: documentRow.hasReadableText

                    Column {
                        id: saveCopyColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        spacing: 4

                        Row {
                            id: saveCopyRow
                            spacing: 8

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Save a copy as"
                                color: JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }

                            Repeater {
                                model: proDocsPage.professionalDocumentListViewModel
                                    .selectableConversionFormats()

                                delegate: Rectangle {
                                    id: saveCopyButton

                                    required property var modelData

                                    width: saveCopyButtonLabel.implicitWidth + 20
                                    height: 24
                                    radius: 12
                                    color: saveCopyButtonMouseArea.containsMouse
                                        ? JobCrushTheme.accentColor : "transparent"
                                    border.width: 1
                                    border.color: JobCrushTheme.accentColor

                                    Text {
                                        id: saveCopyButtonLabel
                                        anchors.centerIn: parent
                                        text: proDocsPage.professionalDocumentListViewModel
                                            .conversionFormatButtonName(saveCopyButton.modelData)
                                        color: saveCopyButtonMouseArea.containsMouse
                                            ? JobCrushTheme.onAccentTextColor
                                            : JobCrushTheme.accentColor
                                        font.pixelSize: JobCrushTheme.smallFontSize
                                    }

                                    MouseArea {
                                        id: saveCopyButtonMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: proDocsPage.professionalDocumentListViewModel
                                            .saveCopyOfDocumentAt(documentRow.index,
                                                                  saveCopyButton.modelData)
                                    }
                                }
                            }

                            // "Want it to look right? Moonlight can lay it out
                            // properly." Shown only when a brain is connected.
                            Rectangle {
                                id: layOutButton

                                readonly property bool canBePressed:
                                    !proDocsPage.professionalDocumentListViewModel.cleanupIsRunning

                                visible: proDocsPage.professionalDocumentListViewModel.brainCanCleanUp
                                width: layOutLabel.implicitWidth + 20
                                height: 24
                                radius: 12
                                color: layOutMouseArea.containsMouse && canBePressed
                                    ? JobCrushTheme.accentColor : "transparent"
                                border.width: 1
                                border.color: JobCrushTheme.accentColor
                                opacity: canBePressed ? 1.0 : 0.5

                                Text {
                                    id: layOutLabel
                                    anchors.centerIn: parent
                                    text: "Word, laid out by Moonlight"
                                    color: layOutMouseArea.containsMouse && layOutButton.canBePressed
                                        ? JobCrushTheme.onAccentTextColor
                                        : JobCrushTheme.accentColor
                                    font.pixelSize: JobCrushTheme.smallFontSize
                                }

                                MouseArea {
                                    id: layOutMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: layOutButton.canBePressed
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: proDocsPage.professionalDocumentListViewModel
                                        .layOutDocumentAt(documentRow.index)
                                }
                            }
                        }

                        // The limit, said before the button is pressed rather
                        // than discovered when the file opens.
                        Text {
                            width: saveCopyColumn.width
                            visible: saveCopyHoverArea.containsMouse
                            text: proDocsPage.professionalDocumentListViewModel
                                .warningAboutConversion()
                            color: JobCrushTheme.mutedTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            wrapMode: Text.Wrap
                        }
                    }

                    // Anchoring rules: an item may only anchor to its parent or a
                    // sibling, and saveCopyRow is a child of saveCopyColumn. So this
                    // anchors to the column (a sibling) and copies the row's size,
                    // which lands on the same rectangle anchors.fill would have.
                    MouseArea {
                        id: saveCopyHoverArea
                        anchors.left: saveCopyColumn.left
                        anchors.top: saveCopyColumn.top
                        anchors.margins: -4
                        width: saveCopyRow.width + 8
                        height: saveCopyRow.height + 8
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }
            }

            MouseArea {
                id: documentRowMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }
    }

    VerticalScrollBar {
        visible: proDocsPage.activeTabName === "documents"
        anchors.top: documentListView.top
        anchors.bottom: documentListView.bottom
        anchors.right: parent.right
        anchors.rightMargin: 18
        flickableTarget: documentListView
    }

    // Nothing dropped yet — say exactly how to change that.
    Rectangle {
        anchors.centerIn: documentListView
        visible: proDocsPage.activeTabName === "documents" && documentListView.count === 0
        width: 440
        height: noDocumentsColumn.implicitHeight + 48
        radius: 12
        color: JobCrushTheme.panelBackgroundColor
        border.color: JobCrushTheme.hairlineBorderColor
        border.width: 1

        Column {
            id: noDocumentsColumn
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 12

            Text {
                width: parent.width
                text: "Nothing here yet"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize + 2
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: "Drag your resume onto this window — anywhere in Job Crush, "
                      + "including the chat with Moonlight — and let go. Job Crush "
                      + "keeps its own copy, so yours stays exactly where you put it."
                color: JobCrushTheme.secondaryTextColor
                font.pixelSize: JobCrushTheme.bodyFontSize
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: "Word documents, PDFs, plain text and pictures."
                color: JobCrushTheme.mutedTextColor
                font.pixelSize: JobCrushTheme.smallFontSize
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // ------------------------------------------------------------------
    // Experience & Education
    //
    // Everything here is Job Crush's READING of a document, not a fact.
    // Reading a resume is a best guess, and it will sometimes be wrong. So
    // every field can be edited, and every entry asks the user to tick it
    // once it is right.
    // ------------------------------------------------------------------
    Flickable {
        id: careerHistoryFlickable

        visible: proDocsPage.activeTabName === "experience"
        anchors.top: proDocsTabRow.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 28
        anchors.rightMargin: 46
        anchors.bottomMargin: 20
        contentWidth: width
        contentHeight: careerHistoryColumn.implicitHeight + 40
        clip: true

        // The same faster wheel as the Documents tab. See the steps there.
        WheelHandler {
            // Qt reports one wheel notch as 120 units of angleDelta.
            readonly property real angleDeltaPerNotch: 120
            readonly property real pixelsPerNotch: 120
            readonly property real speedMultiplier: 2.0
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(wheelEvent) {
                const scrollableDistance = Math.max(
                    0, careerHistoryFlickable.contentHeight - careerHistoryFlickable.height)
                if (scrollableDistance <= 0) {
                    return
                }
                careerHistoryFlickable.contentY = Math.max(
                    careerHistoryFlickable.originY,
                    Math.min(careerHistoryFlickable.originY + scrollableDistance,
                             careerHistoryFlickable.contentY
                             - (wheelEvent.angleDelta.y / angleDeltaPerNotch)
                               * pixelsPerNotch * speedMultiplier))
            }
        }

        Column {
            id: careerHistoryColumn
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 10

            // ---- What this page is, and how to refresh it ----
            Rectangle {
                width: parent.width
                height: careerIntroColumn.implicitHeight + 28
                radius: 8
                color: JobCrushTheme.panelBackgroundColor
                border.color: JobCrushTheme.hairlineBorderColor
                border.width: 1

                Column {
                    id: careerIntroColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: rereadDocumentsButton.left
                    anchors.margins: 14
                    anchors.rightMargin: 14
                    spacing: 4

                    Text {
                        width: parent.width
                        text: "This is what Job Crush read out of your documents."
                        color: JobCrushTheme.primaryTextColor
                        font.pixelSize: JobCrushTheme.bodyFontSize
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: "It gets things wrong sometimes — resumes are all written "
                              + "differently. Fix anything that's off by typing over it, "
                              + "then tick it. Anything you tick or type yourself is kept "
                              + "for good and is never overwritten when documents are "
                              + "read again."
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: "Documents you drop in are read straight away — you "
                              + "shouldn't need the button. It's there for when you want "
                              + "Job Crush to take another run at everything, and it only "
                              + "replaces the entries it guessed."
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }
                }

                Rectangle {
                    id: rereadDocumentsButton
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    width: rereadDocumentsLabel.implicitWidth + 28
                    height: 34
                    radius: 8
                    color: rereadDocumentsMouseArea.containsMouse
                        ? Qt.lighter(JobCrushTheme.accentColor, 1.12) : JobCrushTheme.accentColor

                    Text {
                        id: rereadDocumentsLabel
                        anchors.centerIn: parent
                        text: "Read my documents again"
                        color: JobCrushTheme.onAccentTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: rereadDocumentsMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: proDocsPage.professionalDocumentListViewModel
                            .rereadEveryDocument()
                    }
                }
            }

            // ================= JOBS =================
            Item { width: 1; height: 6 }

            Row {
                width: parent.width
                spacing: 12

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "JOBS"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: proDocsPage.workExperienceListViewModel.unconfirmedCount > 0
                    text: proDocsPage.workExperienceListViewModel.unconfirmedCount
                          + " still need a look"
                    color: JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                // Shuts every card on the page down to its one-line summary.
                // With every card shut, the user can see the whole list at once
                // and check that nothing is MISSING. That is hard to do while
                // scrolling past full-height cards.
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "shut them all"
                    color: shutThemAllMouseArea.containsMouse
                        ? JobCrushTheme.accentColor : JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.underline: shutThemAllMouseArea.containsMouse

                    MouseArea {
                        id: shutThemAllMouseArea
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: proDocsPage.setEveryCareerCardExpanded(false)
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "open them all"
                    color: openThemAllMouseArea.containsMouse
                        ? JobCrushTheme.accentColor : JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    font.underline: openThemAllMouseArea.containsMouse

                    MouseArea {
                        id: openThemAllMouseArea
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: proDocsPage.setEveryCareerCardExpanded(true)
                    }
                }
            }

            Repeater {
                model: proDocsPage.workExperienceListViewModel

                delegate: Rectangle {
                    id: workExperienceCard

                    required property int index
                    required property string employerName
                    required property string roleTitle
                    required property string startDateText
                    required property string endDateText
                    required property string summaryText
                    required property string sourceLineText
                    required property bool isConfirmedByUser

                    // A card the user has ticked starts shut. A card still
                    // waiting on the user starts open, so what is left to
                    // check is in plain sight.
                    property bool isExpanded: !isConfirmedByUser

                    // The one line a shut card shows: title, employer and
                    // dates. Enough to tell if the entry is right without
                    // opening it. Each part is left out when it is empty.
                    readonly property string glanceLine: {
                        const title = roleTitle.length > 0 ? roleTitle : "Untitled job"
                        const employer = employerName.length > 0
                            ? "  ·  " + employerName : ""
                        const dateRange = (startDateText.length > 0 || endDateText.length > 0)
                            ? "  ·  " + startDateText
                              + (endDateText.length > 0 ? " – " + endDateText : "")
                            : ""
                        return title + employer + dateRange
                    }

                    // Opens or shuts this card when "open them all" or
                    // "shut them all" is pressed.
                    Connections {
                        target: proDocsPage
                        function onSetEveryCareerCardExpanded(shouldExpand) {
                            workExperienceCard.isExpanded = shouldExpand
                        }
                    }

                    width: careerHistoryColumn.width
                    height: workExperienceColumn.implicitHeight + 28
                    radius: 8
                    color: JobCrushTheme.panelBackgroundColor
                    border.width: workExperienceCard.isConfirmedByUser ? 1 : 2
                    border.color: workExperienceCard.isConfirmedByUser
                        ? JobCrushTheme.hairlineBorderColor : JobCrushTheme.noticeTextColor

                    Column {
                        id: workExperienceColumn
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 14
                        spacing: 10

                        // ---- The header. Always visible, open or shut. ----
                        //
                        // The confirm tick lives up here rather than down in
                        // the body, so a whole list can be checked off without
                        // opening a single card.
                        Item {
                            width: parent.width
                            height: 24

                            Row {
                                anchors.left: parent.left
                                anchors.right: workExperienceRemoveButton.left
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12
                                    text: workExperienceCard.isExpanded ? "\u25BE" : "\u25B8"
                                    color: JobCrushTheme.secondaryTextColor
                                    font.pixelSize: JobCrushTheme.smallFontSize
                                }

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 20
                                    height: 20
                                    radius: 5
                                    color: workExperienceCard.isConfirmedByUser
                                        ? JobCrushTheme.positiveColor : "transparent"
                                    border.width: 2
                                    border.color: workExperienceCard.isConfirmedByUser
                                        ? JobCrushTheme.positiveColor
                                        : JobCrushTheme.secondaryTextColor

                                    Text {
                                        anchors.centerIn: parent
                                        visible: workExperienceCard.isConfirmedByUser
                                        text: "\u2713"
                                        color: JobCrushTheme.onAccentTextColor
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: proDocsPage.workExperienceListViewModel
                                            .setConfirmedAt(workExperienceCard.index,
                                                            !workExperienceCard.isConfirmedByUser)
                                    }
                                }

                                // 60 pixels are left for the arrow, the tick
                                // box and the gaps between them.
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 60
                                    text: workExperienceCard.glanceLine
                                    textFormat: Text.PlainText
                                    color: workExperienceCard.roleTitle.length > 0
                                        ? JobCrushTheme.primaryTextColor
                                        : JobCrushTheme.mutedTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }

                            // Anywhere on the header opens and shuts the card —
                            // except the tick and the ×, which sit above it.
                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                cursorShape: Qt.PointingHandCursor
                                onClicked: workExperienceCard.isExpanded
                                    = !workExperienceCard.isExpanded
                            }

                            Text {
                                id: workExperienceRemoveButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: "\u00D7"
                                color: removeWorkExperienceMouseArea.containsMouse
                                    ? JobCrushTheme.callToActionColor
                                    : JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.titleFontSize

                                MouseArea {
                                    id: removeWorkExperienceMouseArea
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: proDocsPage.workExperienceListViewModel
                                        .removeWorkExperienceAt(workExperienceCard.index)
                                }
                            }
                        }

                        // ---- The body. Only when it is wanted. ----
                        Column {
                            width: parent.width
                            visible: workExperienceCard.isExpanded
                            spacing: 8

                            // Four boxes on one line. The 36 pixels are the
                            // three 12-pixel gaps between them. What is left
                            // is split 42, 42, 8 and 8 percent.
                            Row {
                                width: parent.width
                                spacing: 12

                                InlineEditField {
                                    width: (parent.width - 36) * 0.42
                                    labelText: "Job title"
                                    placeholderText: "what you were called"
                                    fieldText: workExperienceCard.roleTitle
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.workExperienceListViewModel
                                            .setRoleTitleAt(workExperienceCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.42
                                    labelText: "Employer"
                                    placeholderText: "who you worked for"
                                    fieldText: workExperienceCard.employerName
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.workExperienceListViewModel
                                            .setEmployerNameAt(workExperienceCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.08
                                    labelText: "From"
                                    placeholderText: "2016"
                                    fieldText: workExperienceCard.startDateText
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.workExperienceListViewModel
                                            .setStartDateTextAt(workExperienceCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.08
                                    labelText: "To"
                                    placeholderText: "2019"
                                    fieldText: workExperienceCard.endDateText
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.workExperienceListViewModel
                                            .setEndDateTextAt(workExperienceCard.index, newText)
                                    }
                                }
                            }

                            // Grows to hold whatever is in it, so a whole
                            // paragraph from a resume can be read at once.
                            InlineEditField {
                                width: parent.width
                                labelText: "What you did there"
                                placeholderText: "the short version"
                                allowsMultipleLines: true
                                fieldText: workExperienceCard.summaryText
                                onEditingCommitted: function(newText) {
                                    proDocsPage.workExperienceListViewModel
                                        .setSummaryTextAt(workExperienceCard.index, newText)
                                }
                            }

                            // The line in the document this entry was read
                            // from. When a guess is wrong, the user can see
                            // what Job Crush was reading.
                            Text {
                                width: parent.width
                                visible: workExperienceCard.sourceLineText.length > 0
                                text: "read from your document: \u201C"
                                      + workExperienceCard.sourceLineText + "\u201D"
                                textFormat: Text.PlainText
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                                wrapMode: Text.Wrap
                            }

                            Text {
                                text: workExperienceCard.isConfirmedByUser
                                    ? "Checked by you" : "Tick the box above once this is right"
                                color: workExperienceCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor
                                    : JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: addJobLabel.implicitWidth + 28
                height: 32
                radius: 8
                color: "transparent"
                border.color: JobCrushTheme.hairlineBorderColor
                border.width: 1

                Text {
                    id: addJobLabel
                    anchors.centerIn: parent
                    text: "+  Add a job by hand"
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: proDocsPage.workExperienceListViewModel.addEmptyWorkExperience()
                }
            }

            // ================= SCHOOLING =================
            Item { width: 1; height: 14 }

            Row {
                width: parent.width
                spacing: 12

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "SCHOOLING"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: proDocsPage.educationListViewModel.unconfirmedCount > 0
                    text: proDocsPage.educationListViewModel.unconfirmedCount
                          + " still need a look"
                    color: JobCrushTheme.noticeTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }
            }

            Repeater {
                model: proDocsPage.educationListViewModel

                delegate: Rectangle {
                    id: educationCard

                    required property int index
                    required property string schoolName
                    required property string credentialText
                    required property string fieldOfStudyText
                    required property string startDateText
                    required property string endDateText
                    required property string sourceLineText
                    required property bool isConfirmedByUser

                    // Starts shut once ticked, open while it still needs a
                    // look. The same rule as the job cards above.
                    property bool isExpanded: !isConfirmedByUser

                    // The one line a shut card shows: school, degree,
                    // subject and the year it was finished. Each part is
                    // left out when it is empty.
                    readonly property string glanceLine: {
                        const school = schoolName.length > 0 ? schoolName : "Unnamed school"
                        const credential = credentialText.length > 0
                            ? "  ·  " + credentialText : ""
                        const subject = fieldOfStudyText.length > 0
                            ? "  ·  " + fieldOfStudyText : ""
                        const finished = endDateText.length > 0 ? "  ·  " + endDateText : ""
                        return school + credential + subject + finished
                    }

                    Connections {
                        target: proDocsPage
                        function onSetEveryCareerCardExpanded(shouldExpand) {
                            educationCard.isExpanded = shouldExpand
                        }
                    }

                    width: careerHistoryColumn.width
                    height: educationColumn.implicitHeight + 28
                    radius: 8
                    color: JobCrushTheme.panelBackgroundColor
                    border.width: educationCard.isConfirmedByUser ? 1 : 2
                    border.color: educationCard.isConfirmedByUser
                        ? JobCrushTheme.hairlineBorderColor : JobCrushTheme.noticeTextColor

                    Column {
                        id: educationColumn
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
                                anchors.right: educationRemoveButton.left
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12
                                    text: educationCard.isExpanded ? "\u25BE" : "\u25B8"
                                    color: JobCrushTheme.secondaryTextColor
                                    font.pixelSize: JobCrushTheme.smallFontSize
                                }

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 20
                                    height: 20
                                    radius: 5
                                    color: educationCard.isConfirmedByUser
                                        ? JobCrushTheme.positiveColor : "transparent"
                                    border.width: 2
                                    border.color: educationCard.isConfirmedByUser
                                        ? JobCrushTheme.positiveColor
                                        : JobCrushTheme.secondaryTextColor

                                    Text {
                                        anchors.centerIn: parent
                                        visible: educationCard.isConfirmedByUser
                                        text: "\u2713"
                                        color: JobCrushTheme.onAccentTextColor
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: proDocsPage.educationListViewModel
                                            .setConfirmedAt(educationCard.index,
                                                            !educationCard.isConfirmedByUser)
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 60
                                    text: educationCard.glanceLine
                                    textFormat: Text.PlainText
                                    color: educationCard.schoolName.length > 0
                                        ? JobCrushTheme.primaryTextColor
                                        : JobCrushTheme.mutedTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                cursorShape: Qt.PointingHandCursor
                                onClicked: educationCard.isExpanded = !educationCard.isExpanded
                            }

                            Text {
                                id: educationRemoveButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: "\u00D7"
                                color: removeEducationMouseArea.containsMouse
                                    ? JobCrushTheme.callToActionColor
                                    : JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.titleFontSize

                                MouseArea {
                                    id: removeEducationMouseArea
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: proDocsPage.educationListViewModel
                                        .removeEducationRecordAt(educationCard.index)
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            visible: educationCard.isExpanded
                            spacing: 8

                            // Four boxes on one line, as on the job cards.
                            // What is left after the gaps is split 38, 24, 24
                            // and 14 percent.
                            Row {
                                width: parent.width
                                spacing: 12

                                InlineEditField {
                                    width: (parent.width - 36) * 0.38
                                    labelText: "School"
                                    placeholderText: "where you studied"
                                    fieldText: educationCard.schoolName
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.educationListViewModel
                                            .setSchoolNameAt(educationCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.24
                                    labelText: "Degree or certificate"
                                    placeholderText: "B.S., diploma…"
                                    fieldText: educationCard.credentialText
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.educationListViewModel
                                            .setCredentialTextAt(educationCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.24
                                    labelText: "Subject"
                                    placeholderText: "what you studied"
                                    fieldText: educationCard.fieldOfStudyText
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.educationListViewModel
                                            .setFieldOfStudyTextAt(educationCard.index, newText)
                                    }
                                }

                                InlineEditField {
                                    width: (parent.width - 36) * 0.14
                                    labelText: "Finished"
                                    placeholderText: "2001"
                                    fieldText: educationCard.endDateText
                                    onEditingCommitted: function(newText) {
                                        proDocsPage.educationListViewModel
                                            .setEndDateTextAt(educationCard.index, newText)
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                visible: educationCard.sourceLineText.length > 0
                                text: "read from your document: \u201C"
                                      + educationCard.sourceLineText + "\u201D"
                                textFormat: Text.PlainText
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                                wrapMode: Text.Wrap
                            }

                            Text {
                                text: educationCard.isConfirmedByUser
                                    ? "Checked by you" : "Tick the box above once this is right"
                                color: educationCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor
                                    : JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: addSchoolingLabel.implicitWidth + 28
                height: 32
                radius: 8
                color: "transparent"
                border.color: JobCrushTheme.hairlineBorderColor
                border.width: 1

                Text {
                    id: addSchoolingLabel
                    anchors.centerIn: parent
                    text: "+  Add schooling by hand"
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: proDocsPage.educationListViewModel.addEmptyEducationRecord()
                }
            }
        }
    }

    VerticalScrollBar {
        visible: proDocsPage.activeTabName === "experience"
        anchors.top: careerHistoryFlickable.top
        anchors.bottom: careerHistoryFlickable.bottom
        anchors.right: parent.right
        anchors.rightMargin: 18
        flickableTarget: careerHistoryFlickable
    }
}
