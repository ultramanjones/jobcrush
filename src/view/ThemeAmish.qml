import QtQuick

// Amish: the daylight theme, and the only light one.
//
// Muslin is the paper. Buggy Black is the ink. Morning Straw carries the Saved
// stage so it shows up all the time, not only when something goes wrong.
//
// There is no green in this palette, so "positive" uses Dark Walnut Wood. It is
// dark enough to read as text on paper, which a warm gold is not.
ThemePalette {
    themeName: "amish"
    displayLabel: "Amish"
    isLightTheme: true

    appBackgroundColor: "#F2EFE9"       // Plain Muslin, the paper
    // A full wall of Sunday Blue, the way a quilt uses a block. If this reads
    // as too much, the previous version was Morning Straw at "#DFCFA8".
    // Changing this one line is the whole revert.
    sidebarBackgroundColor: "#3B5973"   // Amish Sunday Blue
    panelBackgroundColor: "#FAF7F0"     // muslin, for the straw to sit against
    cardBackgroundColor: "#E4D9BE"      // a Morning Straw block
    // Sits on the blue sidebar, so it lifts instead of going pale. Going pale
    // would look like a hole cut in the wall.
    sidebarSelectedRowColor: "#4B6E8C"
    hairlineBorderColor: "#2B2B2B"      // Buggy Black. Every block gets a seam.

    primaryTextColor: "#2B2B2B"         // Buggy Black, ink on paper
    secondaryTextColor: "#5C5248"       // walnut gray, still easy to read
    mutedTextColor: "#8A8177"           // faded ink
    onAccentTextColor: "#F2EFE9"        // muslin on red, blue or walnut fills

    sidebarPrimaryTextColor: "#F6F3EC"
    sidebarSecondaryTextColor: "#CBD8E4"
    sidebarMutedTextColor: "#93A9BC"
    sidebarWordmarkColor: "#FFCF6B"

    accentColor: "#3B5973"              // Amish Sunday Blue
    callToActionColor: "#6B1A1B"        // Barn Red
    positiveColor: "#4A321E"            // Dark Walnut Wood
    pendingColor: "#9A9086"
    positiveColorName: "walnut-brown"

    stageSavedColor: "#D6B35A"          // Morning Straw, freshly gathered
    stageAppliedColor: "#4A321E"
    stageInterviewColor: "#3B5973"
    stageOfferColor: "#6B1A1B"          // Barn Red
    stageClosedColor: "#9A9086"

    humanBubbleColor: "#DDE4EC"         // Sunday Blue, washed out to a paper tint
    brainBubbleColor: "#FBF9F5"
    // Morning Straw, darkened so a warning still reads as text on pale paper.
    noticeTextColor: "#8A6B1F"
}
