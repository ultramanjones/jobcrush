pragma Singleton
import QtQuick

// JobCrushTheme
//
// The one theme object every component reads — no component ever hardcodes a
// color, so palettes are swappable by design. Three ship today:
//
//   "classic"    — retro-80s neons and pastels on near-black (default)
//   "grayscale"  — same dark base, distinguished by shade, for sensitive eyes
//                  (and quietly colorblind-friendly)
//   "fruitloops" — the loud one: red, yellow, orange, lime and purple
//
// Main.qml binds activeThemeName to the preferences viewmodel; everything
// else just reads the semantic tokens below and re-renders on change.
QtObject {
    id: theme

    property string activeThemeName: "classic"

    readonly property bool isGrayscale: activeThemeName === "grayscale"
    readonly property bool isFruitLoops: activeThemeName === "fruitloops"

    // --- Base surfaces (dark first, always — every palette) ---------------
    //
    // Fruit Loops leans its darks very slightly purple so the bright cereal
    // colors sit on something that belongs with them rather than on the same
    // neutral near-black as everything else.
    readonly property color appBackgroundColor: isFruitLoops ? "#120A18" : "#0D0F14"
    readonly property color sidebarBackgroundColor: isFruitLoops ? "#170D20" : "#101319"
    readonly property color panelBackgroundColor: isFruitLoops ? "#1D1129" : "#151922"
    readonly property color cardBackgroundColor: isFruitLoops ? "#261635" : "#1B202C"
    readonly property color hairlineBorderColor: isFruitLoops ? "#3A2350" : "#262C3A"

    // --- Text -------------------------------------------------------------
    readonly property color primaryTextColor: isFruitLoops ? "#FFF6E5" : "#E8EAED"
    readonly property color secondaryTextColor: isFruitLoops ? "#C4A8D8" : "#8A93A5"
    readonly property color mutedTextColor: isFruitLoops ? "#8A6BA3" : "#5A6272"

    // --- Accents -----------------------------------------------------------
    //
    // Fruit Loops mapping (Ultra's colors, assigned by job):
    //   accent        = Lively Orange  — the "you are here" color, everywhere
    //   call to action= Bright Red     — the button you are meant to press
    //   positive      = Lime Green     — connected, matched, good news
    // Sunny Yellow and Deep Purple carry the pipeline stages below.
    readonly property color accentColor:
        isGrayscale ? "#C9CED9" : (isFruitLoops ? "#FF6600" : "#35D6EE")        // cyan
    readonly property color callToActionColor:
        isGrayscale ? "#E8EAED" : (isFruitLoops ? "#FF0033" : "#FF3D8A")        // hot pink
    readonly property color positiveColor:
        isGrayscale ? "#AEB6C4" : (isFruitLoops ? "#66CC33" : "#3DF08C")        // terminal green

    // --- Pipeline stage colors (used by the Job Pipelines board in Phase 4;
    //     defined now so every palette is complete from day one) -----------
    readonly property color stageSavedColor:
        isGrayscale ? "#7A8290" : (isFruitLoops ? "#FFCC00" : "#8A93A5")        // Sunny Yellow
    readonly property color stageAppliedColor:
        isGrayscale ? "#9AA2B1" : (isFruitLoops ? "#66CC33" : "#3DF08C")        // green
    readonly property color stageInterviewColor:
        isGrayscale ? "#C9CED9" : (isFruitLoops ? "#FF6600" : "#35D6EE")        // cyan
    readonly property color stageOfferColor:
        isGrayscale ? "#F0F2F5" : (isFruitLoops ? "#FF0033" : "#FF3D8A")        // pink
    readonly property color stageClosedColor: isFruitLoops ? "#660099" : "#5A6272"

    // --- Brain Chat -------------------------------------------------------
    readonly property color humanBubbleColor:
        isGrayscale ? "#232936" : (isFruitLoops ? "#331A47" : "#20303E")
    readonly property color brainBubbleColor: isFruitLoops ? "#261635" : "#1B202C"
    readonly property color noticeTextColor:
        isGrayscale ? "#AEB6C4" : (isFruitLoops ? "#FFCC00" : "#FFB86B")        // Sunny Yellow

    // --- Type scale -------------------------------------------------------
    readonly property int titleFontSize: 22
    readonly property int bodyFontSize: 15
    readonly property int smallFontSize: 12
}
