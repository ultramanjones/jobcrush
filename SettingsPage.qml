import QtQuick

// SettingsPage
//
// AIBrain credential roster, soul-file access, and appearance. Everything
// is instant-apply by law — no Apply/OK ceremony anywhere on this page.
// Pure view: binds to AiCredentialRosterViewModel and AppPreferencesViewModel.
Rectangle {
    id: settingsPage

    // Injected from Main.
    property var credentialRosterViewModel
    property var preferencesViewModel

    color: JobCrushTheme.appBackgroundColor

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: settingsColumn.implicitHeight + 64
        clip: true

        Column {
            id: settingsColumn
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 28
            spacing: 32

            Text {
                text: "Settings"
                color: JobCrushTheme.primaryTextColor
                font.pixelSize: JobCrushTheme.titleFontSize
                font.weight: Font.DemiBold
            }

            // ==========================================================
            // AIBrain credential roster
            // ==========================================================
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: "AIBrain — credential roster"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }

                Text {
                    width: parent.width
                    text: "Pick a provider, drop in its key, done — green means "
                          + "connected. AIBrain speaks Anthropic today; OpenAI and "
                          + "Ollama are on the way."
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }

                // ---- Add a key ---------------------------------------
                Rectangle {
                    width: parent.width
                    height: addKeyColumn.implicitHeight + 32
                    radius: 8
                    color: JobCrushTheme.panelBackgroundColor
                    border.color: JobCrushTheme.hairlineBorderColor
                    border.width: 1

                    Column {
                        id: addKeyColumn
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 16
                        spacing: 12

                        // Provider chips.
                        Row {
                            spacing: 8

                            Repeater {
                                model: ["anthropic", "openai", "ollama"]

                                delegate: Rectangle {
                                    id: providerChip

                                    required property string modelData

                                    readonly property bool isSelected:
                                        settingsPage.newKeyProviderKindName === modelData

                                    // Green chip = this provider already holds a key.
                                    // (Reading rosterRevision makes this binding
                                    // re-evaluate on every roster change.)
                                    readonly property bool slotIsFilled: {
                                        settingsPage.credentialRosterViewModel.rosterRevision
                                        return settingsPage.credentialRosterViewModel
                                            .providerHasKey(providerChip.modelData)
                                    }

                                    width: providerChipLabel.implicitWidth + 24
                                    height: 30
                                    radius: 15
                                    color: isSelected
                                        ? (slotIsFilled ? JobCrushTheme.positiveColor
                                                        : JobCrushTheme.accentColor)
                                        : "transparent"
                                    border.color: slotIsFilled
                                        ? JobCrushTheme.positiveColor
                                        : (isSelected ? JobCrushTheme.accentColor
                                                      : JobCrushTheme.hairlineBorderColor)
                                    border.width: 1

                                    Text {
                                        id: providerChipLabel
                                        anchors.centerIn: parent
                                        // The dot marks a filled slot at a glance.
                                        text: (providerChip.slotIsFilled ? "● " : "")
                                              + providerChip.modelData
                                        color: providerChip.isSelected
                                            ? "#0D0F14"
                                            : (providerChip.slotIsFilled
                                                   ? JobCrushTheme.positiveColor
                                                   : JobCrushTheme.secondaryTextColor)
                                        font.pixelSize: JobCrushTheme.smallFontSize
                                        font.weight: providerChip.isSelected
                                            ? Font.DemiBold : Font.Normal
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: settingsPage.newKeyProviderKindName
                                            = providerChip.modelData
                                    }
                                }
                            }
                        }

                        // API key field. Two states:
                        //  - empty slot: a real input, type the key in
                        //  - filled slot: asterisks in a green frame — the key
                        //    is registered, on display but never revealed
                        Rectangle {
                            width: parent.width
                            height: 40
                            radius: 8
                            color: JobCrushTheme.appBackgroundColor
                            border.color: settingsPage.selectedProviderHasKey
                                ? JobCrushTheme.positiveColor
                                : (secretKeyInput.activeFocus
                                       ? JobCrushTheme.accentColor
                                       : JobCrushTheme.hairlineBorderColor)
                            border.width: 1

                            TextInput {
                                id: secretKeyInput
                                anchors.fill: parent
                                anchors.margins: 10
                                verticalAlignment: TextInput.AlignVCenter
                                visible: !settingsPage.selectedProviderHasKey
                                color: JobCrushTheme.primaryTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                                echoMode: TextInput.Password
                                clip: true
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                visible: !settingsPage.selectedProviderHasKey
                                         && secretKeyInput.text.length === 0
                                         && !secretKeyInput.activeFocus
                                text: "API key"
                                color: JobCrushTheme.mutedTextColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                            }

                            // The filled-slot display.
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                visible: settingsPage.selectedProviderHasKey
                                text: "••••••••••••••••••••"
                                color: JobCrushTheme.positiveColor
                                font.pixelSize: JobCrushTheme.bodyFontSize
                                font.letterSpacing: 2
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                visible: settingsPage.selectedProviderHasKey
                                text: "key registered"
                                color: JobCrushTheme.positiveColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                            }
                        }

                        // The helping hand: opens the provider's own official
                        // page for getting a key — nobody should have to
                        // google their way in.
                        Text {
                            visible: !settingsPage.selectedProviderHasKey
                            text: "Where do I get a " + settingsPage.newKeyProviderKindName
                                  + " key?  <a href=\"open\">Open the official page</a>"
                            textFormat: Text.RichText
                            linkColor: JobCrushTheme.accentColor
                            color: JobCrushTheme.secondaryTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            onLinkActivated: settingsPage.credentialRosterViewModel
                                .openProviderKeyInstructions(settingsPage.newKeyProviderKindName)

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink !== ""
                                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 12

                            // Nickname: an input while adding, a plain display
                            // once the slot is filled.
                            Rectangle {
                                width: parent.width - actionButton.width - 12
                                height: 40
                                radius: 8
                                color: JobCrushTheme.appBackgroundColor
                                border.color: settingsPage.selectedProviderHasKey
                                    ? JobCrushTheme.positiveColor
                                    : (displayLabelInput.activeFocus
                                           ? JobCrushTheme.accentColor
                                           : JobCrushTheme.hairlineBorderColor)
                                border.width: 1

                                TextInput {
                                    id: displayLabelInput
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    verticalAlignment: TextInput.AlignVCenter
                                    visible: !settingsPage.selectedProviderHasKey
                                    color: JobCrushTheme.primaryTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                    clip: true
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    visible: !settingsPage.selectedProviderHasKey
                                             && displayLabelInput.text.length === 0
                                             && !displayLabelInput.activeFocus
                                    text: "Key nickname  (\"my personal key\")"
                                    color: JobCrushTheme.mutedTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                }

                                // Filled slot: show the nickname they gave it.
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    visible: settingsPage.selectedProviderHasKey
                                    text: {
                                        settingsPage.credentialRosterViewModel.rosterRevision
                                        return settingsPage.credentialRosterViewModel
                                            .providerKeyNickname(settingsPage.newKeyProviderKindName)
                                    }
                                    color: JobCrushTheme.primaryTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                }
                            }

                            // One button, two jobs: "Add key" for an empty
                            // slot, "Delete key" for a filled one.
                            Rectangle {
                                id: actionButton

                                readonly property bool slotIsFilled:
                                    settingsPage.selectedProviderHasKey
                                readonly property bool addIsPossible:
                                    secretKeyInput.text.trim().length > 0

                                width: actionButtonLabel.implicitWidth + 32
                                height: 40
                                radius: 8
                                color: slotIsFilled
                                    ? (actionButtonMouseArea.containsMouse
                                           ? Qt.lighter(JobCrushTheme.callToActionColor, 1.12)
                                           : JobCrushTheme.callToActionColor)
                                    : (addIsPossible
                                           ? (actionButtonMouseArea.containsMouse
                                                  ? Qt.lighter(JobCrushTheme.positiveColor, 1.12)
                                                  : JobCrushTheme.positiveColor)
                                           : JobCrushTheme.cardBackgroundColor)

                                Text {
                                    id: actionButtonLabel
                                    anchors.centerIn: parent
                                    text: actionButton.slotIsFilled ? "Delete key" : "Add key"
                                    color: (actionButton.slotIsFilled || actionButton.addIsPossible)
                                        ? "#0D0F14" : JobCrushTheme.mutedTextColor
                                    font.pixelSize: JobCrushTheme.bodyFontSize
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    id: actionButtonMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: actionButton.slotIsFilled || actionButton.addIsPossible
                                    cursorShape: enabled
                                        ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        if (actionButton.slotIsFilled) {
                                            settingsPage.credentialRosterViewModel
                                                .deleteProviderKey(settingsPage.newKeyProviderKindName)
                                            return
                                        }
                                        settingsPage.credentialRosterViewModel.addCredential(
                                            settingsPage.newKeyProviderKindName,
                                            secretKeyInput.text,
                                            displayLabelInput.text)
                                        secretKeyInput.clear()
                                        displayLabelInput.clear()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ==========================================================
            // The soul
            // ==========================================================
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: "AIBrain — the soul"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }

                Text {
                    width: parent.width
                    text: "The agent's identity lives in plain text files — prime "
                          + "directives (hard law) and soul (personality plus your "
                          + "voice profile). Edit them with any editor; the next "
                          + "request reads the new soul. No recompiling."
                    color: JobCrushTheme.secondaryTextColor
                    font.pixelSize: JobCrushTheme.smallFontSize
                    wrapMode: Text.Wrap
                }

                Row {
                    spacing: 12

                    Rectangle {
                        width: openSoulFolderLabel.implicitWidth + 28
                        height: 36
                        radius: 8
                        color: openSoulFolderMouseArea.containsMouse
                            ? Qt.lighter(JobCrushTheme.accentColor, 1.12)
                            : JobCrushTheme.accentColor

                        Text {
                            id: openSoulFolderLabel
                            anchors.centerIn: parent
                            text: "Open soul folder"
                            color: "#0D0F14"
                            font.pixelSize: JobCrushTheme.bodyFontSize
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: openSoulFolderMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settingsPage.preferencesViewModel.openSoulFolder()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: settingsPage.preferencesViewModel.soulFolderPath
                        color: JobCrushTheme.mutedTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        elide: Text.ElideMiddle
                        width: Math.min(implicitWidth, settingsColumn.width - 220)
                    }
                }
            }

            // ==========================================================
            // Appearance
            // ==========================================================
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: "Appearance — board theme"
                    color: JobCrushTheme.accentColor
                    font.pixelSize: JobCrushTheme.bodyFontSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }

                Row {
                    spacing: 8

                    Repeater {
                        model: [
                            { themeName: "classic",   displayLabel: "Classic Job Crush" },
                            { themeName: "grayscale", displayLabel: "Grayscale" }
                        ]

                        delegate: Rectangle {
                            id: themeChip

                            required property var modelData

                            readonly property bool isSelected:
                                settingsPage.preferencesViewModel.boardThemeName
                                    === modelData.themeName

                            width: themeChipLabel.implicitWidth + 28
                            height: 34
                            radius: 17
                            color: isSelected ? JobCrushTheme.accentColor : "transparent"
                            border.color: isSelected
                                ? JobCrushTheme.accentColor
                                : JobCrushTheme.hairlineBorderColor
                            border.width: 1

                            Text {
                                id: themeChipLabel
                                anchors.centerIn: parent
                                text: themeChip.modelData.displayLabel
                                color: themeChip.isSelected
                                    ? "#0D0F14" : JobCrushTheme.secondaryTextColor
                                font.pixelSize: JobCrushTheme.smallFontSize
                                font.weight: themeChip.isSelected
                                    ? Font.DemiBold : Font.Normal
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsPage.preferencesViewModel.boardThemeName
                                    = themeChip.modelData.themeName
                            }
                        }
                    }
                }
            }
        }
    }

    // The provider chip selection for the key form (view-local UI state).
    property string newKeyProviderKindName: "anthropic"

    // Whether the selected provider's slot already holds a key. Reading
    // rosterRevision makes this re-evaluate on every roster change.
    readonly property bool selectedProviderHasKey: {
        credentialRosterViewModel.rosterRevision
        return credentialRosterViewModel.providerHasKey(newKeyProviderKindName)
    }
}
