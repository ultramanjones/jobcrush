import QtQuick

// ProDocsPage
//
// Professional Documentation — everything the user has handed Job Crush, and
// what it understood from it. Two tabs: the documents themselves, and the
// experience and education pulled out of them.
//
// This page is also a drop target by nature, but the basket that catches
// files lives on the window (see Main.qml) so a drop works from any screen.
// Building it here would mean the one time someone drops a resume on Brain
// Chat, nothing happens and they conclude the feature is broken.
Rectangle {
    id: proDocsPage

    // Injected from Main.
    property var professionalDocumentListViewModel
    property var workExperienceListViewModel
    property var educationListViewModel

    color: JobCrushTheme.appBackgroundColor

    // "documents" | "experience"
    property string activeTabName: "documents"

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

        WheelHandler {
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
                             - (wheelEvent.angleDelta.y / 120) * pixelsPerNotch * speedMultiplier))
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

            // What kind Job Crush thinks it is — and a click cycles it, because
            // the guess is a heuristic and correcting it must be trivial.
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
                    onClicked: {
                        const kinds = proDocsPage.professionalDocumentListViewModel
                            .selectableDocumentKinds()
                        const nextIndex =
                            (kinds.indexOf(documentRow.documentKind) + 1) % kinds.length
                        proDocsPage.professionalDocumentListViewModel
                            .setDocumentKindAt(documentRow.index, kinds[nextIndex])
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
                    // Word count is the honest measure of whether it loaded.
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
    // Everything here started life as Job Crush's READING of a document, not
    // as a fact. Every field is editable and every entry asks to be confirmed,
    // because parsing a resume is a heuristic that will sometimes be wrong —
    // and an app that is wrong and cannot be corrected is worse than one that
    // never guessed at all.
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

        WheelHandler {
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
                             - (wheelEvent.angleDelta.y / 120) * pixelsPerNotch * speedMultiplier))
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
                        spacing: 8

                        Row {
                            width: parent.width
                            spacing: 12

                            InlineEditField {
                                width: (parent.width - 24) * 0.42
                                labelText: "Job title"
                                placeholderText: "what you were called"
                                fieldText: workExperienceCard.roleTitle
                                onEditingCommitted: function(newText) {
                                    proDocsPage.workExperienceListViewModel
                                        .setRoleTitleAt(workExperienceCard.index, newText)
                                }
                            }

                            InlineEditField {
                                width: (parent.width - 24) * 0.42
                                labelText: "Employer"
                                placeholderText: "who you worked for"
                                fieldText: workExperienceCard.employerName
                                onEditingCommitted: function(newText) {
                                    proDocsPage.workExperienceListViewModel
                                        .setEmployerNameAt(workExperienceCard.index, newText)
                                }
                            }

                            InlineEditField {
                                width: (parent.width - 24) * 0.08
                                labelText: "From"
                                placeholderText: "2016"
                                fieldText: workExperienceCard.startDateText
                                onEditingCommitted: function(newText) {
                                    proDocsPage.workExperienceListViewModel
                                        .setStartDateTextAt(workExperienceCard.index, newText)
                                }
                            }

                            InlineEditField {
                                width: (parent.width - 24) * 0.08
                                labelText: "To"
                                placeholderText: "2019"
                                fieldText: workExperienceCard.endDateText
                                onEditingCommitted: function(newText) {
                                    proDocsPage.workExperienceListViewModel
                                        .setEndDateTextAt(workExperienceCard.index, newText)
                                }
                            }
                        }

                        InlineEditField {
                            width: parent.width
                            labelText: "What you did there"
                            placeholderText: "the short version"
                            fieldText: workExperienceCard.summaryText
                            onEditingCommitted: function(newText) {
                                proDocsPage.workExperienceListViewModel
                                    .setSummaryTextAt(workExperienceCard.index, newText)
                            }
                        }

                        // Where this came from. A wrong guess should always be
                        // traceable rather than merely doubted.
                        Text {
                            width: parent.width
                            visible: workExperienceCard.sourceLineText.length > 0
                            text: "read from your document: “" + workExperienceCard.sourceLineText + "”"
                            color: JobCrushTheme.mutedTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            elide: Text.ElideRight
                        }

                        Row {
                            spacing: 10

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20
                                height: 20
                                radius: 5
                                color: workExperienceCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : "transparent"
                                border.width: 2
                                border.color: workExperienceCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : JobCrushTheme.secondaryTextColor

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

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: workExperienceCard.isConfirmedByUser
                                    ? "Checked by you" : "That's right — keep it"
                                color: workExperienceCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        text: "×"
                        color: removeWorkExperienceMouseArea.containsMouse
                            ? JobCrushTheme.callToActionColor : JobCrushTheme.mutedTextColor
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
                        spacing: 8

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
                            text: "read from your document: “" + educationCard.sourceLineText + "”"
                            color: JobCrushTheme.mutedTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            elide: Text.ElideRight
                        }

                        Row {
                            spacing: 10

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20
                                height: 20
                                radius: 5
                                color: educationCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : "transparent"
                                border.width: 2
                                border.color: educationCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : JobCrushTheme.secondaryTextColor

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
                                text: educationCard.isConfirmedByUser
                                    ? "Checked by you" : "That's right — keep it"
                                color: educationCard.isConfirmedByUser
                                    ? JobCrushTheme.positiveColor : JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        text: "×"
                        color: removeEducationMouseArea.containsMouse
                            ? JobCrushTheme.callToActionColor : JobCrushTheme.mutedTextColor
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
