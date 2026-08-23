pragma Singleton
import QtQuick

// JobCrushTheme
//
// The one theme object every component reads — no component ever hardcodes a
// color, so palettes are swappable by design. Five ship today:
//
//   "classic"    — retro-80s neons and pastels on near-black (default)
//   "grayscale"  — same dark base, distinguished by shade, for sensitive eyes
//                  (and quietly colorblind-friendly)
//   "fruitloops" — the loud one: red, yellow, orange, lime and purple
//   "amish"      — the daylight one, and the only light palette: muslin
//                  paper, morning straw, buggy black ink, Sunday blue,
//                  barn red and walnut
//   "traceon"    — the digital void: cyan program glow on black, with
//                  hostile orange and circuit green
//
// Main.qml binds activeThemeName to the preferences viewmodel; everything
// else just reads the semantic tokens below and re-renders on change.
//
// Adding a palette means adding one branch per token and a chip in Settings.
// Nothing else in the app changes, which is the whole point.
QtObject {
    id: theme

    property string activeThemeName: "classic"

    readonly property bool isGrayscale: activeThemeName === "grayscale"
    readonly property bool isFruitLoops: activeThemeName === "fruitloops"
    readonly property bool isAmish: activeThemeName === "amish"
    readonly property bool isTraceOn: activeThemeName === "traceon"

    // Amish is the one palette that is light rather than dark. Anything that
    // needs to know which way round it is asks HERE rather than checking for
    // a theme by name — otherwise every future light palette means hunting
    // down the places that assumed dark.
    readonly property bool isLightTheme: isAmish

    // --- Base surfaces (dark first, always — every palette) ---------------
    readonly property color appBackgroundColor: {
        if (isFruitLoops) return "#120A18"
        if (isAmish)      return "#F2EFE9"   // Plain Muslin — paper
        if (isTraceOn)    return "#050505"   // deep digital void
        return "#0D0F14"
    }
    readonly property color sidebarBackgroundColor: {
        if (isFruitLoops) return "#170D20"
        // Amish Sunday Blue, a full wall of it, the way a quilt uses a block.
        // If this reads as too much, the previous version was Morning Straw
        // at "#DFCFA8" — swapping the line back is the whole revert.
        if (isAmish)      return "#3B5973"
        if (isTraceOn)    return "#080A0C"
        return "#101319"
    }
    readonly property color panelBackgroundColor: {
        if (isFruitLoops) return "#1D1129"
        if (isAmish)      return "#FAF7F0"   // muslin, for the straw to sit against
        if (isTraceOn)    return "#0B0F12"
        return "#151922"
    }
    readonly property color cardBackgroundColor: {
        if (isFruitLoops) return "#261635"
        if (isAmish)      return "#E4D9BE"   // a Morning Straw block
        if (isTraceOn)    return "#101619"
        return "#1B202C"
    }
    // The panel behind the current sidebar row. On Amish it sits ON the blue,
    // so it lifts rather than lightening to muslin, which would look like a
    // hole cut in the wall.
    readonly property color sidebarSelectedRowColor:
        isAmish ? "#4B6E8C" : panelBackgroundColor

    readonly property color hairlineBorderColor: {
        if (isFruitLoops) return "#3A2350"
        if (isAmish)      return "#2B2B2B"   // Buggy Black — quilt binding, not a
                                             // whisper. Every block gets a seam.
        if (isTraceOn)    return "#3A4A5A"   // inactive wireframe gray
        return "#262C3A"
    }

    // --- Sidebar text ------------------------------------------------------
    //
    // Its own tokens because the sidebar is the one surface that can carry a
    // bold block of color while the rest of the page stays pale. Without
    // these, dark ink on Amish's Sunday Blue wall would be unreadable, and
    // the whole idea would be off the table.
    readonly property color sidebarPrimaryTextColor:
        isAmish ? "#F6F3EC" : primaryTextColor
    readonly property color sidebarSecondaryTextColor:
        isAmish ? "#CBD8E4" : secondaryTextColor
    readonly property color sidebarMutedTextColor:
        isAmish ? "#93A9BC" : mutedTextColor
    readonly property color sidebarWordmarkColor:
        isAmish ? "#FFCF6B" : callToActionColor

    // --- Text -------------------------------------------------------------
    readonly property color primaryTextColor: {
        if (isFruitLoops) return "#FFF6E5"
        if (isAmish)      return "#2B2B2B"   // Buggy Black — ink on paper
        if (isTraceOn)    return "#FFFFFF"   // high-energy core
        return "#E8EAED"
    }
    readonly property color secondaryTextColor: {
        if (isFruitLoops) return "#C4A8D8"
        if (isAmish)      return "#5C5248"   // walnut-gray, still comfortably readable
        if (isTraceOn)    return "#8FA6B8"
        return "#8A93A5"
    }
    readonly property color mutedTextColor: {
        if (isFruitLoops) return "#8A6BA3"
        if (isAmish)      return "#8A8177"   // faded ink
        if (isTraceOn)    return "#5A6C7A"
        return "#5A6272"
    }

    // Text and glyphs that sit ON a filled accent or call-to-action shape.
    //
    // This exists because a palette can be dark-on-bright (neon buttons) or
    // bright-on-dark (Barn Red buttons), and a component cannot know which.
    // Before this token every button hardcoded near-black label text, which
    // was invisible the moment a palette used a dark button.
    readonly property color onAccentTextColor: {
        if (isAmish)   return "#F2EFE9"   // muslin on barn red, blue or walnut fills
        if (isTraceOn) return "#050505"   // void black on a glowing program
        return "#0D0F14"
    }

    // --- Accents -----------------------------------------------------------
    //
    // Amish note: this is the daylight palette. Muslin is the paper, Buggy
    // Black is the ink, and Morning Straw carries the sidebar and the Saved
    // stage so it is visible constantly rather than only when something goes
    // wrong. The palette ships no green, so "positive" takes Dark Walnut
    // Wood — solid, and dark enough to read as text on paper, which a warm
    // gold simply is not. Where a token has to be a shade rather than one of
    // the six exact colors, it is a tint of one of them and says so.
    readonly property color accentColor: {
        if (isGrayscale)  return "#C9CED9"
        if (isFruitLoops) return "#FF6600"   // Lively Orange
        if (isAmish)      return "#3B5973"   // Amish Sunday Blue, at full strength
        if (isTraceOn)    return "#00F0FF"   // standard program glow
        return "#35D6EE"                     // cyan
    }
    readonly property color callToActionColor: {
        if (isGrayscale)  return "#E8EAED"
        if (isFruitLoops) return "#FF0033"   // Bright Red
        if (isAmish)      return "#6B1A1B"   // Barn Red
        if (isTraceOn)    return "#FF2A00"   // hostile program glow
        return "#FF3D8A"                     // hot pink
    }
    readonly property color positiveColor: {
        if (isGrayscale)  return "#AEB6C4"
        if (isFruitLoops) return "#66CC33"   // Lime Green
        if (isAmish)      return "#4A321E"   // Dark Walnut Wood — solid, and dark
                                             // enough to read as text on paper
        if (isTraceOn)    return "#00FF66"   // secondary circuit paths
        return "#3DF08C"                     // terminal green
    }

    // --- Pipeline stage colors (used by the Job Pipelines board in Phase 4;
    //     defined now so every palette is complete from day one) -----------
    readonly property color stageSavedColor: {
        if (isGrayscale)  return "#7A8290"
        if (isFruitLoops) return "#FFCC00"   // Sunny Yellow
        if (isAmish)      return "#D6B35A"   // Morning Straw — freshly gathered
        if (isTraceOn)    return "#3A4A5A"
        return "#8A93A5"
    }
    readonly property color stageAppliedColor: {
        if (isGrayscale)  return "#9AA2B1"
        if (isFruitLoops) return "#66CC33"
        if (isAmish)      return "#4A321E"
        if (isTraceOn)    return "#00FF66"
        return "#3DF08C"
    }
    readonly property color stageInterviewColor: {
        if (isGrayscale)  return "#C9CED9"
        if (isFruitLoops) return "#FF6600"
        if (isAmish)      return "#3B5973"
        if (isTraceOn)    return "#00F0FF"
        return "#35D6EE"
    }
    readonly property color stageOfferColor: {
        if (isGrayscale)  return "#F0F2F5"
        if (isFruitLoops) return "#FF0033"
        if (isAmish)      return "#6B1A1B"   // Barn Red
        if (isTraceOn)    return "#FF2A00"
        return "#FF3D8A"
    }
    readonly property color stageClosedColor: {
        if (isFruitLoops) return "#660099"   // Deep Purple
        if (isAmish)      return "#9A9086"
        if (isTraceOn)    return "#3A4A5A"
        return "#5A6272"
    }

    // --- Brain Chat -------------------------------------------------------
    readonly property color humanBubbleColor: {
        if (isGrayscale)  return "#232936"
        if (isFruitLoops) return "#331A47"
        if (isAmish)      return "#DDE4EC"   // Sunday Blue, washed out to a paper tint
        if (isTraceOn)    return "#08262B"
        return "#20303E"
    }
    readonly property color brainBubbleColor: {
        if (isFruitLoops) return "#261635"
        if (isAmish)      return "#FBF9F5"
        if (isTraceOn)    return "#101619"
        return "#1B202C"
    }
    readonly property color noticeTextColor: {
        if (isGrayscale)  return "#AEB6C4"
        if (isFruitLoops) return "#FFCC00"
        if (isAmish)      return "#8A6B1F"   // Morning Straw, darkened so a warning
                                             // still reads as text on pale paper
        if (isTraceOn)    return "#FF2A00"
        return "#FFB86B"
    }

    // --- Glow -------------------------------------------------------------
    //
    // How strongly lit shapes bleed into the dark around them, 0 to 1. Only
    // Trace On lights up; every other palette sits flat at zero so nothing
    // else in the app changes appearance because this token exists.
    readonly property real glowStrength: isTraceOn ? 1.0 : 0.0

    // --- Type scale -------------------------------------------------------
    readonly property int titleFontSize: 22
    readonly property int bodyFontSize: 15
    readonly property int smallFontSize: 12
}
