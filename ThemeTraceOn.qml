import QtQuick

// Trace On: the digital void. Cyan program glow on black, with hostile orange
// and circuit green.
ThemePalette {
    themeName: "traceon"
    displayLabel: "Trace On"

    appBackgroundColor: "#050505"       // deep digital void
    sidebarBackgroundColor: "#080A0C"
    panelBackgroundColor: "#0B0F12"
    cardBackgroundColor: "#101619"
    hairlineBorderColor: "#3A4A5A"      // inactive wireframe gray

    primaryTextColor: "#FFFFFF"         // high-energy core
    secondaryTextColor: "#8FA6B8"
    mutedTextColor: "#5A6C7A"

    accentColor: "#00F0FF"              // standard program glow
    callToActionColor: "#FF2A00"        // hostile program glow
    positiveColor: "#00FF66"            // secondary circuit paths
    pendingColor: "#3A4A5A"
    positiveColorName: "green"

    stageSavedColor: "#3A4A5A"
    stageAppliedColor: "#00FF66"
    stageInterviewColor: "#00F0FF"
    stageOfferColor: "#FF2A00"
    stageClosedColor: "#3A4A5A"

    humanBubbleColor: "#08262B"
    brainBubbleColor: "#101619"
    noticeTextColor: "#FF2A00"

    glowStrength: 1.0
}
