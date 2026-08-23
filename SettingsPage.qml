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
    property var brainConnectionViewModel

    color: JobCrushTheme.appBackgroundColor

    // Opening Settings is one of the KEY MOMENTS a connection check exists
    // for. A cached result answers instantly; only a genuine change (new key,
    // different brain) costs a request. Job Crush never polls a vendor.
    onVisibleChanged: {
        if (visible) {
            brainConnectionViewModel.checkConnectionNow()
        }
    }

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

            // ==========================================================
            // The loudest thing on the page: which brain is actually live.
            // Green means a real request just came back from the vendor —
            // never a guess, never a leftover from the last launch.
            // ==========================================================
            Rectangle {
                width: parent.width
                height: activeBrainBannerColumn.implicitHeight + 28
                radius: 10
                color: JobCrushTheme.panelBackgroundColor
                border.width: 1
                border.color: settingsPage.brainConnectionViewModel.brainIsConnectedAndActive
                    ? JobCrushTheme.positiveColor
                    : JobCrushTheme.hairlineBorderColor

                // A stripe of the same truth, for anyone reading shape before
                // words.
                Rectangle {
                    width: 4
                    height: parent.height - 20
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    radius: 2
                    color: settingsPage.brainConnectionViewModel.brainIsConnectedAndActive
                        ? JobCrushTheme.positiveColor
                        : JobCrushTheme.noticeTextColor
                }

                Column {
                    id: activeBrainBannerColumn
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    spacing: 5

                    Text {
                        id: activeBrainBannerLabel
                        width: parent.width
                        text: settingsPage.brainConnectionViewModel.bannerText
                        color: settingsPage.brainConnectionViewModel.brainIsConnectedAndActive
                            ? JobCrushTheme.positiveColor
                            : JobCrushTheme.noticeTextColor
                        font.pixelSize: JobCrushTheme.titleFontSize
                        font.weight: Font.Bold
                        font.letterSpacing: 1.5
                        wrapMode: Text.Wrap

                        // Confirming a connection is a real wait, so the words
                        // say what is happening and breathe while it happens.
                        // (No spinner. Ever.)
                        SequentialAnimation on opacity {
                            running: settingsPage.brainConnectionViewModel.connectionIsBeingChecked
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.4; duration: 700 }
                            NumberAnimation { from: 0.4; to: 1.0; duration: 700 }
                            onRunningChanged: {
                                if (!running) {
                                    activeBrainBannerLabel.opacity = 1.0
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: settingsPage.brainConnectionViewModel.statusDetailText
                        color: JobCrushTheme.secondaryTextColor
                        font.pixelSize: JobCrushTheme.smallFontSize
                        wrapMode: Text.Wrap
                    }
                }
            }

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
                    text: "Two separate things live here. The TABS pick whose key "
                          + "you're editing — a green ● means that slot already holds "
                          + "one. The CHECKBOX picks which brain actually answers you. "
                          + "AIBrain speaks Anthropic and OpenRouter today; OpenAI, "
                          + "Gemini, and Ollama are on the way."
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

                        // Provider tabs, each with the checkbox that IS the
                        // brain choice. The tab picks which key slot the form
                        // below edits; the checkbox picks which brain actually
                        // answers. Two jobs, two separate targets — so looking
                        // at another provider's key never switches your brain
                        // out from under you.
                        Row {
                            spacing: 18

                            Repeater {
                                model: ["anthropic", "openrouter", "openai", "gemini", "ollama"]

                                delegate: Row {
                                    id: providerEntry

                                    required property string modelData

                                    spacing: 7

                                    // Reading connectionRevision / rosterRevision makes
                                    // every binding below re-evaluate whenever anything
                                    // underneath moves.
                                    readonly property bool canBeSelected: {
                                        settingsPage.brainConnectionViewModel.connectionRevision
                                        settingsPage.credentialRosterViewModel.rosterRevision
                                        return settingsPage.brainConnectionViewModel
                                            .providerCanBeSelected(providerEntry.modelData)
                                    }
                                    readonly property bool isSelectedBrain: {
                                        settingsPage.brainConnectionViewModel.connectionRevision
                                        return settingsPage.brainConnectionViewModel
                                            .providerIsSelected(providerEntry.modelData)
                                    }
                                    readonly property bool isSelectedAndActive: {
                                        settingsPage.brainConnectionViewModel.connectionRevision
                                        return settingsPage.brainConnectionViewModel
                                            .providerIsSelectedAndActive(providerEntry.modelData)
                                    }
                                    readonly property bool slotIsFilled: {
                                        settingsPage.credentialRosterViewModel.rosterRevision
                                        return settingsPage.credentialRosterViewModel
                                            .providerHasKey(providerEntry.modelData)
                                    }
                                    readonly property bool isFocusedTab:
                                        settingsPage.newKeyProviderKindName === providerEntry.modelData

                                    // ---- The checkbox: the user's choice of brain ----
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 22
                                        height: 22
                                        radius: 5
                                        color: providerEntry.isSelectedAndActive
                                            ? JobCrushTheme.positiveColor : "transparent"
                                        border.width: 2
                                        border.color: !providerEntry.canBeSelected
                                            ? JobCrushTheme.hairlineBorderColor
                                            : (providerEntry.isSelectedBrain
                                                   ? JobCrushTheme.positiveColor
                                                   : JobCrushTheme.secondaryTextColor)
                                        opacity: providerEntry.canBeSelected ? 1.0 : 0.45

                                        Text {
                                            id: brainChoiceTick
                                            anchors.centerIn: parent
                                            visible: providerEntry.isSelectedBrain
                                            text: "\u2713"
                                            color: providerEntry.isSelectedAndActive
                                                ? "#0D0F14" : JobCrushTheme.positiveColor
                                            font.pixelSize: 15
                                            font.weight: Font.Bold

                                            // Chosen but not confirmed yet: the tick
                                            // breathes rather than sitting there
                                            // claiming to be live.
                                            SequentialAnimation on opacity {
                                                running: providerEntry.isSelectedBrain
                                                         && !providerEntry.isSelectedAndActive
                                                loops: Animation.Infinite
                                                NumberAnimation { from: 1.0; to: 0.3; duration: 650 }
                                                NumberAnimation { from: 0.3; to: 1.0; duration: 650 }
                                                onRunningChanged: {
                                                    if (!running) {
                                                        brainChoiceTick.opacity = 1.0
                                                    }
                                                }
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            enabled: providerEntry.canBeSelected
                                            cursorShape: enabled
                                                ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: settingsPage.brainConnectionViewModel
                                                .setProviderSelected(providerEntry.modelData,
                                                                     !providerEntry.isSelectedBrain)
                                        }
                                    }

                                    // ---- The tab: which key slot the form edits ----
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: providerTabLabel.implicitWidth + 24
                                        height: 30
                                        radius: 15
                                        color: providerEntry.isFocusedTab
                                            ? JobCrushTheme.cardBackgroundColor : "transparent"
                                        border.color: providerEntry.isFocusedTab
                                            ? JobCrushTheme.accentColor
                                            : (providerEntry.slotIsFilled
                                                   ? JobCrushTheme.positiveColor
                                                   : JobCrushTheme.hairlineBorderColor)
                                        // Double thickness on the focused tab. Color alone
                                        // is not enough to carry "this is the one you're
                                        // looking at" — in Grayscale there is no accent hue
                                        // to notice, so the WEIGHT of the border has to say
                                        // it too.
                                        border.width: providerEntry.isFocusedTab ? 2 : 1

                                        Text {
                                            id: providerTabLabel
                                            anchors.centerIn: parent
                                            // The dot marks a filled key slot at a glance.
                                            text: (providerEntry.slotIsFilled ? "\u25CF " : "")
                                                  + providerEntry.modelData
                                            color: providerEntry.slotIsFilled
                                                ? JobCrushTheme.positiveColor
                                                : (providerEntry.isFocusedTab
                                                       ? JobCrushTheme.primaryTextColor
                                                       : JobCrushTheme.secondaryTextColor)
                                            font.pixelSize: JobCrushTheme.smallFontSize
                                            font.weight: providerEntry.isFocusedTab
                                                ? Font.DemiBold : Font.Normal
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: settingsPage.newKeyProviderKindName
                                                = providerEntry.modelData
                                        }
                                    }
                                }
                            }
                        }

                        // Why the focused provider's checkbox is unavailable —
                        // a fact about the situation, so nobody is left
                        // wondering what they did wrong.
                        Text {
                            width: parent.width
                            readonly property string brainUnavailableReason: {
                                settingsPage.brainConnectionViewModel.connectionRevision
                                settingsPage.credentialRosterViewModel.rosterRevision
                                return settingsPage.brainConnectionViewModel
                                    .reasonProviderCannotBeSelected(
                                        settingsPage.newKeyProviderKindName)
                            }
                            visible: brainUnavailableReason.length > 0
                            text: brainUnavailableReason
                            color: JobCrushTheme.mutedTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            wrapMode: Text.Wrap
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

                        // Paste guardrail: if the typed/pasted key looks like
                        // it belongs to a different provider's slot, say so —
                        // masked fields hide paste mistakes, this unhides them.
                        Text {
                            readonly property string warningText:
                                settingsPage.credentialRosterViewModel.keyFormatWarning(
                                    settingsPage.newKeyProviderKindName,
                                    secretKeyInput.text)
                            visible: !settingsPage.selectedProviderHasKey
                                     && warningText.length > 0
                            width: parent.width
                            text: warningText
                            color: JobCrushTheme.noticeTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            wrapMode: Text.Wrap
                        }

                        // The helping hand: opens the provider's own official
                        // page for getting a key — nobody should have to
                        // google their way in.
                        Text {
                            visible: !settingsPage.selectedProviderHasKey
                            // "an anthropic key" but "a gemini key" — the
                            // article follows the provider name's first letter.
                            text: "Where do I get "
                                  + (/^[aeiou]/.test(settingsPage.newKeyProviderKindName) ? "an " : "a ")
                                  + settingsPage.newKeyProviderKindName
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

                        // For a connected provider: the accurate meter is the
                        // provider's own dashboard — link straight to it.
                        Text {
                            visible: settingsPage.selectedProviderHasKey
                                     && settingsPage.credentialRosterViewModel
                                            .providerHasUsagePage(settingsPage.newKeyProviderKindName)
                            text: "Wondering what this key has spent?  "
                                  + "<a href=\"open\">Check usage on "
                                  + settingsPage.newKeyProviderKindName + "'s dashboard</a>"
                            textFormat: Text.RichText
                            linkColor: JobCrushTheme.accentColor
                            color: JobCrushTheme.secondaryTextColor
                            font.pixelSize: JobCrushTheme.smallFontSize
                            onLinkActivated: settingsPage.credentialRosterViewModel
                                .openProviderUsagePage(settingsPage.newKeyProviderKindName)

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
