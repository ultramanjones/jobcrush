import QtQuick
import JobCrush

// GlowHalo
//
// A soft light around a shape, for the Trace On themes. Placed inside the
// shape it lights; it draws itself behind the shape and a few pixels past its
// edges, in fading rings of glowColor.
//
// It reads JobCrushTheme.glowStrength. Every theme but Trace On sets that to
// 0, and then the halo is not drawn at all, so no other theme changes.
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

    // How far the light reaches past the shape's edge, in pixels.
    readonly property int ringCount: 4

    anchors.fill: parent
    z: -1
    visible: isLit && JobCrushTheme.glowStrength > 0

    Repeater {
        model: glowHalo.ringCount

        delegate: Rectangle {
            required property int index

            readonly property int ringSize: (index + 1) * 2

            anchors.fill: parent
            anchors.margins: -ringSize
            radius: glowHalo.cornerRadius + ringSize
            color: "transparent"
            border.width: 2
            border.color: glowHalo.glowColor
            // Brightest at the edge, fading outwards.
            opacity: JobCrushTheme.glowStrength * 0.32 / (index + 1)
        }
    }
}
