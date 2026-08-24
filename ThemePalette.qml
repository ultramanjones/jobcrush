import QtQuick

// ThemePalette
//
// The list of every color and setting a theme has to provide. Each theme is a
// file that starts from this one and fills in the values. See ThemeClassic.qml
// for the shortest example.
//
// Unset colors default to magenta. That is on purpose. If someone adds a token
// here and forgets to set it in a theme, the screen turns bright pink and the
// mistake is found in one second instead of shipping as a wrong shade nobody
// notices.
//
// A few tokens default to another token instead of magenta. Those are the ones
// that are usually the same and only differ in one theme, such as the sidebar
// text colors, which only Amish changes. A theme that needs something else just
// sets it.
QtObject {
    // What this theme is called in settings and in the saved preference.
    property string themeName: "unnamed"
    property string displayLabel: "Unnamed"

    // True for a light theme. Anything that needs to know which way round the
    // colors run asks this instead of checking the theme name, so a future
    // light theme does not mean hunting down every place that assumed dark.
    property bool isLightTheme: false

    // --- Surfaces ---------------------------------------------------------
    property color appBackgroundColor: "#FF00FF"
    property color sidebarBackgroundColor: "#FF00FF"
    property color panelBackgroundColor: "#FF00FF"
    property color cardBackgroundColor: "#FF00FF"
    property color hairlineBorderColor: "#FF00FF"

    // The panel behind the selected sidebar row. Usually the panel color.
    property color sidebarSelectedRowColor: panelBackgroundColor

    // --- Text -------------------------------------------------------------
    property color primaryTextColor: "#FF00FF"
    property color secondaryTextColor: "#FF00FF"
    property color mutedTextColor: "#FF00FF"

    // Text and icons drawn on top of a filled accent or call-to-action shape.
    // A theme can have bright buttons with dark labels or dark buttons with
    // bright labels, and a component cannot tell which. Usually the app
    // background color works.
    property color onAccentTextColor: appBackgroundColor

    // --- Sidebar text -----------------------------------------------------
    //
    // Separate from the main text colors because the sidebar is the one
    // surface that can carry a solid block of color while the rest of the page
    // stays pale. Amish is the reason: dark ink on its blue sidebar would be
    // unreadable. Every other theme uses the same colors as the page.
    property color sidebarPrimaryTextColor: primaryTextColor
    property color sidebarSecondaryTextColor: secondaryTextColor
    property color sidebarMutedTextColor: mutedTextColor
    property color sidebarWordmarkColor: callToActionColor

    // --- Accents ----------------------------------------------------------
    property color accentColor: "#FF00FF"
    property color callToActionColor: "#FF00FF"
    property color positiveColor: "#FF00FF"

    // Something is running and has not finished yet. Colorless in every theme,
    // so "waiting" is never mistaken for "working" at a glance.
    property color pendingColor: "#FF00FF"

    // What positiveColor looks like, in one word.
    //
    // UI text is allowed to name a color. "A green dot means that slot already
    // has a key" is a useful sentence. It is not allowed to hardcode the word,
    // because that sentence is wrong in four of the six themes. Any text that
    // names a color reads it from here.
    property string positiveColorName: "green"

    // --- Pipeline stages --------------------------------------------------
    property color stageSavedColor: "#FF00FF"
    property color stageAppliedColor: "#FF00FF"
    property color stageInterviewColor: "#FF00FF"
    property color stageOfferColor: "#FF00FF"
    property color stageClosedColor: "#FF00FF"

    // --- Brain Chat -------------------------------------------------------
    property color humanBubbleColor: "#FF00FF"
    property color brainBubbleColor: "#FF00FF"
    property color noticeTextColor: "#FF00FF"

    // --- Glow -------------------------------------------------------------
    //
    // How much lit shapes bleed into the dark around them, 0 to 1. Only the
    // Trace On themes light up. Everything else stays at 0, so no other theme
    // changes appearance because this token exists.
    property real glowStrength: 0.0
}
