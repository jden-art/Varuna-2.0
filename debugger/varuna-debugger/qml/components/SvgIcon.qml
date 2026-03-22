// -----------------------------------------------------------------
// File: qml/components/SvgIcon.qml
//
// Reusable SVG icon component. Drop any .svg into assets/icons/
// and use it anywhere in the app like this:
//
//   SvgIcon {
//       name:   "boot"          // loads assets/icons/boot.svg
//       size:   Math.round(20 * rootWindow.dp)
//       color:  Theme.primary   // recolors the SVG (works on monochrome SVGs)
//   }
//
// The SVG is rendered via Qt's built-in SVG engine. No external
// library is needed — PySide6 ships with QtSvg support.
//
// COLORING:
//   Qt's Image element cannot directly recolor SVG paths. We achieve
//   theme-reactive coloring by layering the SVG over a solid rectangle
//   and using the Image as an alpha mask (via OpacityMask from
//   Qt5Compat.GraphicalEffects, which ships with PySide6 6.5+).
//
//   If you need the SVG to contain multiple colors, set useColorMask
//   to false — the SVG will render with its own embedded colors.
//
// SVG REQUIREMENTS for mask coloring to work:
//   - Paths must be filled with a dark color (black or near-black).
//     White fills will be invisible after masking.
//   - The SVG viewBox should be square (e.g. 0 0 24 24).
//   - No external raster images embedded in the SVG.
//   - Keep the SVG simple — complex filters or gradients will render
//     but won't recolor correctly through the mask.
// -----------------------------------------------------------------

import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: svgIcon

    // ── Public API ────────────────────────────────────────────────

    // Filename without extension: "boot" → assets/icons/boot.svg
    property string name: ""

    // Rendered size (width = height = size)
    property real size: Math.round(20 * (parent ? parent.width / 1280 : 1))

    // Tint color applied via mask. Ignored when useColorMask is false.
    property color color: "white"

    // Set false to render the SVG's own embedded colors unchanged.
    property bool useColorMask: true

    // Smooth scaling (leave true for SVGs)
    property bool smooth: true

    // ── Internal sizing ───────────────────────────────────────────

    width:  svgIcon.size
    height: svgIcon.size

    // ── Source path ───────────────────────────────────────────────

    // Qt.resolvedUrl resolves relative to the QML file's location.
    // SvgIcon.qml is in qml/components/, so we go up two levels to
    // the project root, then into assets/icons/.
    readonly property url _src: name.length > 0
        ? Qt.resolvedUrl("../../assets/icons/" + name + ".svg")
        : ""

    // ── Masked rendering (default — single-color recoloring) ──────

    // The solid fill rectangle — this provides the color
    Rectangle {
        id: colorLayer
        anchors.fill: parent
        color: svgIcon.color
        visible: false   // hidden; used only as source for OpacityMask

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    // The SVG image — used as the alpha mask shape
    Image {
        id: maskImage
        anchors.fill:  parent
        source:        svgIcon._src
        visible:       false
        smooth:        svgIcon.smooth
        mipmap:        true
        fillMode:      Image.PreserveAspectFit
        antialiasing:  true
        sourceSize.width:  svgIcon.size * 2   // render at 2× for crisp edges
        sourceSize.height: svgIcon.size * 2
    }

    // Composite: colorLayer through maskImage's alpha channel
    OpacityMask {
        anchors.fill:  parent
        source:        colorLayer
        maskSource:    maskImage
        visible:       svgIcon.useColorMask && svgIcon.name.length > 0
        invert:        false
    }

    // ── Unmasked rendering (multi-color SVGs) ─────────────────────

    Image {
        anchors.fill:  parent
        source:        svgIcon._src
        smooth:        svgIcon.smooth
        mipmap:        true
        fillMode:      Image.PreserveAspectFit
        antialiasing:  true
        sourceSize.width:  svgIcon.size * 2
        sourceSize.height: svgIcon.size * 2
        visible:       !svgIcon.useColorMask && svgIcon.name.length > 0
    }

    // ── Missing icon fallback ─────────────────────────────────────
    // Shows a red question-mark square if the SVG file is not found.

    Rectangle {
        anchors.fill:  parent
        color:         "transparent"
        border.color:  "#f28b82"
        border.width:  1
        radius:        2
        visible:       svgIcon.name.length > 0
                       && maskImage.status === Image.Error

        Text {
            anchors.centerIn: parent
            text:       "?"
            color:      "#f28b82"
            font.pixelSize: parent.width * 0.6
            font.weight: Font.Bold
        }
    }
}
