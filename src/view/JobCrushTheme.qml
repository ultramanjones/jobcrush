pragma Singleton
import QtQuick

// JobCrushTheme
//
// The one theme object every component reads. No component ever hardcodes a
// color, so themes are swappable.
//
// Each theme is its own file: ThemeClassic.qml, ThemeGrayscale.qml,
// ThemeFruitLoops.qml, ThemeAmish.qml, ThemeTraceOn.qml, ThemeTraceOnTwo.qml.
// They all start from ThemePalette.qml, which lists every token a theme has to
// provide.
//
// This file does two things: it picks which theme is active, and it re-exposes
// that theme's tokens so the rest of the app has one name to read.
//
// To add a theme: write the file, add it to the palettes list below, and add it
// to QML_FILES in CMakeLists.txt. Nothing else in the app changes. Settings
// builds its chips from availableThemes, so the new theme shows up on its own.
//
// Main.qml binds activeThemeName to the preferences viewmodel. Everything else
// reads the tokens below and re-renders when the theme changes.
QtObject {
    id: theme

    property string activeThemeName: "classic"

    // --- The themes -------------------------------------------------------
    //
    // All six exist at once. They are property bags, so this costs nothing,
    // and it means switching themes is a pointer change rather than rebuilding
    // anything.
    readonly property ThemeClassic classicTheme: ThemeClassic {}
    readonly property ThemeGrayscale grayscaleTheme: ThemeGrayscale {}
    readonly property ThemeFruitLoops fruitLoopsTheme: ThemeFruitLoops {}
    readonly property ThemeAmish amishTheme: ThemeAmish {}
    readonly property ThemeTraceOn traceOnTheme: ThemeTraceOn {}
    readonly property ThemeTraceOnTwo traceOnTwoTheme: ThemeTraceOnTwo {}
    readonly property ThemeWraithWranglers wraithWranglersTheme: ThemeWraithWranglers {}

    readonly property var everyTheme: [
        classicTheme, grayscaleTheme, fruitLoopsTheme,
        amishTheme, traceOnTheme, traceOnTwoTheme, wraithWranglersTheme
    ]

    // The theme that is on right now. An unknown name falls back to Classic,
    // so a settings file from an older build cannot leave the app unpainted.
    readonly property ThemePalette activeTheme: {
        for (let i = 0; i < everyTheme.length; ++i) {
            if (everyTheme[i].themeName === activeThemeName) {
                return everyTheme[i]
            }
        }
        return classicTheme
    }

    // The chips in Settings are built from this, so adding a theme does not
    // mean editing SettingsPage.
    readonly property var availableThemes: {
        let list = []
        for (let i = 0; i < everyTheme.length; ++i) {
            list.push({
                themeName: everyTheme[i].themeName,
                displayLabel: everyTheme[i].displayLabel
            })
        }
        return list
    }

    // --- The tokens -------------------------------------------------------
    //
    // Every one of these is the active theme's value. They are listed here so
    // the rest of the app says JobCrushTheme.accentColor and never has to know
    // which theme is on.
    readonly property bool isLightTheme: activeTheme.isLightTheme

    readonly property color appBackgroundColor: activeTheme.appBackgroundColor
    readonly property color sidebarBackgroundColor: activeTheme.sidebarBackgroundColor
    readonly property color panelBackgroundColor: activeTheme.panelBackgroundColor
    readonly property color cardBackgroundColor: activeTheme.cardBackgroundColor
    readonly property color sidebarSelectedRowColor: activeTheme.sidebarSelectedRowColor
    readonly property color hairlineBorderColor: activeTheme.hairlineBorderColor

    readonly property color primaryTextColor: activeTheme.primaryTextColor
    readonly property color secondaryTextColor: activeTheme.secondaryTextColor
    readonly property color mutedTextColor: activeTheme.mutedTextColor
    readonly property color onAccentTextColor: activeTheme.onAccentTextColor

    readonly property color sidebarPrimaryTextColor: activeTheme.sidebarPrimaryTextColor
    readonly property color sidebarSecondaryTextColor: activeTheme.sidebarSecondaryTextColor
    readonly property color sidebarMutedTextColor: activeTheme.sidebarMutedTextColor
    readonly property color sidebarWordmarkColor: activeTheme.sidebarWordmarkColor

    readonly property color accentColor: activeTheme.accentColor
    readonly property color callToActionColor: activeTheme.callToActionColor
    readonly property color positiveColor: activeTheme.positiveColor
    readonly property color pendingColor: activeTheme.pendingColor
    readonly property string positiveColorName: activeTheme.positiveColorName

    readonly property color stageSavedColor: activeTheme.stageSavedColor
    readonly property color stageAppliedColor: activeTheme.stageAppliedColor
    readonly property color stageInterviewColor: activeTheme.stageInterviewColor
    readonly property color stageOfferColor: activeTheme.stageOfferColor
    readonly property color stageClosedColor: activeTheme.stageClosedColor

    readonly property color humanBubbleColor: activeTheme.humanBubbleColor
    readonly property color brainBubbleColor: activeTheme.brainBubbleColor
    readonly property color noticeTextColor: activeTheme.noticeTextColor

    readonly property real glowStrength: activeTheme.glowStrength

    // --- Type scale -------------------------------------------------------
    //
    // The same in every theme, so it lives here instead of in the theme files.
    readonly property int titleFontSize: 22
    readonly property int bodyFontSize: 15
    readonly property int smallFontSize: 12
}
