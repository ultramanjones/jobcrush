import QtQuick
import JobCrush

// GlowHalo
//
// A soft light around a shape, for the Trace On themes. Placed inside the
// shape it lights; it draws itself behind the shape and a few pixels past its
// edges, in fading rings of glowColor.
//
// It reads JobCrushTheme.glowStrength. Every theme but the two Trace On
// themes sets that to 0, and then the halo is not drawn at all, so no other
// theme changes.
//
// Plain rectangles, no shader effects, so it works on every Qt 6 version
// Job Crush builds with.
Item {
    id: glowHalo

    // The color of the light. Usually the color of the shape's own edge.
    property color glowColor: JobCrushTheme.accentColor

    // The corner rounding of the shape being lit, so the rings follow it.
    property real cornerRadius: 0

    // Whether this shape is lit right now. A tab glows only while it is the
    // active tab, for example.
    property bool isLit: true

    // How many rings of light are drawn around the shape.
    readonly property int ringCount: 4

    // The gap between one ring and the next, in pixels. The light reaches
    // ringCount times this far past the shape's edge: 8 pixels today.
    readonly property int pixelsBetweenRings: 2

    // How bright the innermost ring is at full glowStrength, from 0 to 1.
    readonly property real innermostRingOpacity: 0.32

    anchors.fill: parent
    z: -1
    visible: isLit && JobCrushTheme.glowStrength > 0

    Repeater {
        model: glowHalo.ringCount

        delegate: Rectangle {
            required property int index

            // How far this ring sits outside the shape. Ring 0 is 2 pixels
            // out, ring 1 is 4, and so on.
            readonly property int ringSize: (index + 1) * glowHalo.pixelsBetweenRings

            // A negative margin grows the ring past the shape on every side.
            // The corner rounding grows by the same amount, so the ring keeps
            // the shape's outline.
            anchors.fill: parent
            anchors.margins: -ringSize
            radius: glowHalo.cornerRadius + ringSize
            color: "transparent"
            border.width: glowHalo.pixelsBetweenRings
            border.color: glowHalo.glowColor
            // Brightest at the edge, fading outwards. Ring 0 gets the full
            // innermost opacity, ring 1 gets half, ring 2 a third, ring 3 a
            // quarter. All of it is scaled by the theme's glowStrength.
            opacity: JobCrushTheme.glowStrength * glowHalo.innermostRingOpacity / (index + 1)
        }
    }
}
