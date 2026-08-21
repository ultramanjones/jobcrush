pragma Singleton
import QtQuick

// JobCrushTheme
//
// The one theme object every component reads — no component ever hardcodes a
// color, so palettes are swappable by design. v1 ships two:
//
//   "classic"   — retro-80s neons and pastels on near-black (default)
//   "grayscale" — same dark base, distinguished by shade, for sensitive eyes
//                 (and quietly colorblind-friendly)
//
// Main.qml binds activeThemeName to the preferences viewmodel; everything
// else just reads the semantic tokens below and re-renders on change.
QtObject {
    id: theme

    property string activeThemeName: "classic"

    readonly property bool isGrayscale: activeThemeName === "grayscale"

    // --- Base surfaces (shared by both palettes: dark first, always) ------
    readonly property color appBackgroundColor: "#0D0F14"
    readonly property color sidebarBackgroundColor: "#101319"
    readonly property color panelBackgroundColor: "#151922"
    readonly property color cardBackgroundColor: "#1B202C"
    readonly property color hairlineBorderColor: "#262C3A"

    // --- Text -------------------------------------------------------------
    readonly property color primaryTextColor: "#E8EAED"
    readonly property color secondaryTextColor: "#8A93A5"
    readonly property color mutedTextColor: "#5A6272"

    // --- Accents (the retro-80s neons; grays in grayscale) ---------------
    readonly property color accentColor: isGrayscale ? "#C9CED9" : "#35D6EE"        // cyan
    readonly property color callToActionColor: isGrayscale ? "#E8EAED" : "#FF3D8A"  // hot pink
    readonly property color positiveColor: isGrayscale ? "#AEB6C4" : "#3DF08C"      // terminal green

    // --- Pipeline stage colors (used by the board in Phase 4; defined now
    //     so the palette is complete from day one) -------------------------
    readonly property color stageSavedColor: isGrayscale ? "#7A8290" : "#8A93A5"
    readonly property color stageAppliedColor: isGrayscale ? "#9AA2B1" : "#3DF08C"   // green
    readonly property color stageInterviewColor: isGrayscale ? "#C9CED9" : "#35D6EE" // cyan
    readonly property color stageOfferColor: isGrayscale ? "#F0F2F5" : "#FF3D8A"     // pink
    readonly property color stageClosedColor: "#5A6272"

    // --- Brain Chat -------------------------------------------------------
    readonly property color humanBubbleColor: isGrayscale ? "#232936" : "#20303E"
    readonly property color brainBubbleColor: "#1B202C"
    readonly property color noticeTextColor: isGrayscale ? "#AEB6C4" : "#FFB86B"

    // --- Type scale -------------------------------------------------------
    readonly property int titleFontSize: 22
    readonly property int bodyFontSize: 15
    readonly property int smallFontSize: 12
}
