import QtQuick

// Fruit Loops: the loud one. Red, yellow, orange, lime and purple.
ThemePalette {
    themeName: "fruitloops"
    displayLabel: "Fruit Loops"

    appBackgroundColor: "#120A18"
    sidebarBackgroundColor: "#170D20"
    panelBackgroundColor: "#1D1129"
    cardBackgroundColor: "#261635"
    hairlineBorderColor: "#3A2350"

    primaryTextColor: "#FFF6E5"
    secondaryTextColor: "#C4A8D8"
    mutedTextColor: "#8A6BA3"

    // Not this theme's own background. The buttons are bright enough that the
    // near-black from Classic reads better on them.
    onAccentTextColor: "#0D0F14"

    accentColor: "#FF6600"          // Lively Orange
    callToActionColor: "#FF0033"    // Bright Red
    positiveColor: "#66CC33"        // Lime Green
    pendingColor: "#6E5A82"
    positiveColorName: "lime"

    stageSavedColor: "#FFCC00"      // Sunny Yellow
    stageAppliedColor: "#66CC33"
    stageInterviewColor: "#FF6600"
    stageOfferColor: "#FF0033"
    stageClosedColor: "#660099"     // Deep Purple

    humanBubbleColor: "#331A47"
    brainBubbleColor: "#261635"
    noticeTextColor: "#FFCC00"
}
