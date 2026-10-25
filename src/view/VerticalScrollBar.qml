import QtQuick
import JobCrush

// VerticalScrollBar
//
// A hand-rolled scroll bar for any Flickable (a ListView is one). Hand-rolled
// because Job Crush builds its own components rather than importing a control
// set — the front end IS the product.
//
// Deliberately always visible whenever there is anything to scroll, rather
// than the modern habit of fading away until touched. A scroll bar that hides
// itself is a scroll bar the user has to discover, and it also throws away
// the one honest signal it carries for free: how much more there is below.
//
// The thumb's position is a plain binding on the target's contentY, and the
// mouse sets contentY rather than the thumb — so there is no binding fighting
// a drag, and click-anywhere-on-the-track jumps there for free.
Item {
    id: verticalScrollBar

    // The Flickable (or ListView) this scrolls.
    property Flickable flickableTarget: null

    // How far the target can travel. Zero when everything already fits.
    readonly property real scrollableDistance: flickableTarget === null
        ? 0
        : Math.max(0, flickableTarget.contentHeight - flickableTarget.height)

    readonly property bool thereIsSomethingToScroll: scrollableDistance > 0

    width: 10
    visible: thereIsSomethingToScroll

    // The track.
    Rectangle {
        id: scrollTrack
        anchors.fill: parent
        radius: width / 2
        color: JobCrushTheme.panelBackgroundColor
        border.color: JobCrushTheme.hairlineBorderColor
        border.width: 1

        // The thumb. Its length says how much of the whole list is on screen.
        // Its height is the track height times the share of the content that
        // fits on screen, but never shorter than minimumThumbHeight, so a
        // very long list still leaves something to grab.
        //
        // Its y is how far the target has scrolled, as a fraction from 0 to
        // 1, times the room the thumb can move in. contentY is measured from
        // originY, which is not always 0 for a ListView.
        Rectangle {
            id: scrollThumb

            readonly property real minimumThumbHeight: 36

            width: parent.width
            height: verticalScrollBar.flickableTarget === null
                ? minimumThumbHeight
                : Math.max(minimumThumbHeight,
                           scrollTrack.height * (verticalScrollBar.flickableTarget.height
                                                 / verticalScrollBar.flickableTarget.contentHeight))

            y: {
                if (!verticalScrollBar.thereIsSomethingToScroll) {
                    return 0
                }
                const travelledFraction =
                    (verticalScrollBar.flickableTarget.contentY
                     - verticalScrollBar.flickableTarget.originY)
                    / verticalScrollBar.scrollableDistance
                return Math.max(0, Math.min(1, travelledFraction))
                       * (scrollTrack.height - height)
            }

            radius: width / 2
            color: scrollThumbMouseArea.containsMouse || scrollThumbMouseArea.pressed
                ? JobCrushTheme.accentColor
                : JobCrushTheme.secondaryTextColor
        }

        // One mouse area over the whole track: pressing anywhere jumps there
        // and keeps following the pointer, which is drag and click-to-jump in
        // the same three lines.
        MouseArea {
            id: scrollThumbMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            function scrollToPointer(pointerY) {
                if (!verticalScrollBar.thereIsSomethingToScroll) {
                    return
                }
                const usableTrackHeight = scrollTrack.height - scrollThumb.height
                if (usableTrackHeight <= 0) {
                    return
                }
                // Center the thumb on the pointer, so the spot grabbed is the
                // spot that ends up under the cursor. The steps:
                //  1. Subtract half the thumb, so the thumb's middle is what
                //     follows the pointer, not its top edge.
                //  2. Divide by the room the thumb can move in. That gives
                //     how far down the track the pointer is, from 0 to 1.
                //  3. Clamp to 0..1, so a drag past either end stops there.
                //  4. Turn that fraction into a contentY for the target.
                const requestedFraction =
                    (pointerY - scrollThumb.height / 2) / usableTrackHeight
                const clampedFraction = Math.max(0, Math.min(1, requestedFraction))
                verticalScrollBar.flickableTarget.contentY =
                    verticalScrollBar.flickableTarget.originY
                    + clampedFraction * verticalScrollBar.scrollableDistance
            }

            onPressed: function(mouseEvent) { scrollToPointer(mouseEvent.y) }
            onPositionChanged: function(mouseEvent) {
                if (pressed) {
                    scrollToPointer(mouseEvent.y)
                }
            }
        }
    }
}
