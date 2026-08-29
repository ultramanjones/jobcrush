import QtQuick

// Wraith Wranglers: night work in a red-striped ambulance.
//
// Dead Black is the night. Uniform Blue is the coveralls, and it takes the
// sidebar the way Amish takes it with Sunday Blue. Paranormal Mist Blue is the
// glow, so it does the job the other themes give to cyan. Ambulance Red is for
// anything that needs to be seen from down the street. Slime Green is the win.
//
// Where a token needs a shade instead of one of the eight colors, it is a
// darker or lighter version of one of them and says which.
ThemePalette {
    themeName: "wraithwranglers"
    displayLabel: "Wraith Wranglers"

    appBackgroundColor: "#000000"       // Dead Black
    sidebarBackgroundColor: "#191970"   // Uniform Blue, a full wall of it
    panelBackgroundColor: "#0C0C16"     // Dead Black, lifted toward the blue
    cardBackgroundColor: "#15152B"      // Uniform Blue, darkened
    sidebarSelectedRowColor: "#2A2A8C"  // Uniform Blue, lifted. It sits on the
                                        // blue wall, so it brightens instead of
                                        // going dark.
    hairlineBorderColor: "#5A5A5A"      // Proton Pack Gray, dimmed to a seam

    primaryTextColor: "#FFFFFF"         // Ghostly White
    secondaryTextColor: "#A9A9A9"       // Proton Pack Gray
    mutedTextColor: "#6E6E6E"           // Proton Pack Gray, dimmed
    onAccentTextColor: "#000000"        // Dead Black on a mist blue or red fill

    sidebarPrimaryTextColor: "#FFFFFF"
    sidebarSecondaryTextColor: "#B0E0E6"  // Paranormal Mist Blue
    sidebarMutedTextColor: "#8A8AC0"      // Uniform Blue, lightened
    sidebarWordmarkColor: "#B0E0E6"       // Mist blue. Ambulance Red on the blue
                                          // wall is too dark to read.

    accentColor: "#B0E0E6"              // Paranormal Mist Blue, the glow (#76c9d4)
    callToActionColor: "#CC181F"        // Ambulance Red
    positiveColor: "#39FF14"            // Slime Green
    pendingColor: "#A9A9A9"             // Proton Pack Gray
    positiveColorName: "green"

    stageSavedColor: "#6D7B5F"          // Khaki Green, the jumpsuit
    stageAppliedColor: "#39FF14"        // Slime Green
    stageInterviewColor: "#B0E0E6"
    stageOfferColor: "#CC181F"
    stageClosedColor: "#4A4A4A"         // Proton Pack Gray, dimmed

    humanBubbleColor: "#15152B"
    brainBubbleColor: "#0C0C16"
    noticeTextColor: "#CC181F"          // Ambulance Red
}
