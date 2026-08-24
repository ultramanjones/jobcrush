import QtQuick

// Grayscale: the same dark base as Classic, with the accents replaced by
// shades. For sensitive eyes, and it works for colorblind users too.
ThemePalette {
    themeName: "grayscale"
    displayLabel: "Grayscale"

    appBackgroundColor: "#0D0F14"
    sidebarBackgroundColor: "#101319"
    panelBackgroundColor: "#151922"
    cardBackgroundColor: "#1B202C"
    hairlineBorderColor: "#262C3A"

    primaryTextColor: "#E8EAED"
    secondaryTextColor: "#8A93A5"
    mutedTextColor: "#5A6272"

    accentColor: "#C9CED9"
    callToActionColor: "#E8EAED"
    positiveColor: "#AEB6C4"
    pendingColor: "#6B7484"
    positiveColorName: "pale"

    stageSavedColor: "#7A8290"
    stageAppliedColor: "#9AA2B1"
    stageInterviewColor: "#C9CED9"
    stageOfferColor: "#F0F2F5"
    stageClosedColor: "#5A6272"

    humanBubbleColor: "#232936"
    brainBubbleColor: "#1B202C"
    noticeTextColor: "#AEB6C4"
}
