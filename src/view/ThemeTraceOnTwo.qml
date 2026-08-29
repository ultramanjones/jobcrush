import QtQuick

// Trace On II: the sequel's grid. Good Cyan and Clueless Yellow over Grid Sky,
// with Recognizer Red for the things that bite.
ThemePalette {
    themeName: "traceon2"
    displayLabel: "Trace On II"

    appBackgroundColor: "#061722"       // Grid Sky, sunk to the horizon
    sidebarBackgroundColor: "#0E3E55"   // Grid Sky, a full wall of it
    panelBackgroundColor: "#0B2C3D"     // Grid Sky, lifted one step
    cardBackgroundColor: "#124B65"      // Grid Sky, lifted two steps
    hairlineBorderColor: "#7E99A4"      // Digital Skin Gray, the seams

    primaryTextColor: "#FEF3F7"         // Glowy White
    secondaryTextColor: "#7E99A4"       // Digital Skin Gray
    mutedTextColor: "#587683"           // Digital Skin Gray, dimmed
    onAccentTextColor: "#0E3E55"        // Grid Sky ink on a lit program

    accentColor: "#f0b432"              // Clueless Yellow
    callToActionColor: "#C43041"        // Recognizer Red, lit enough to read
                                        // against Grid Sky
    positiveColor: "#6DEBFA"            // Good Cyan, the good programs
    pendingColor: "#7E99A4"
    positiveColorName: "cyan"

    stageSavedColor: "#7E99A4"
    stageAppliedColor: "#6DEBFA"
    stageInterviewColor: "#F7DC97"
    stageOfferColor: "#C43041"
    stageClosedColor: "#41626F"

    humanBubbleColor: "#123A4E"         // Grid Sky, one step toward the light
    brainBubbleColor: "#0B2C3D"
    noticeTextColor: "#E8737F"          // Recognizer Red, lit for a warning

    glowStrength: 1.0
}
