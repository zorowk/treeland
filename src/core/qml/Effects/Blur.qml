// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Waylib.Server
import Treeland

RenderBufferBlitter {
    id: blitter
    smooth: true

    property real radius: 0
    property rect frameRect: Qt.rect(0, 0, width, height)
    property bool radiusEnabled: radius > 0
    property QtObject effectConfig: WaylandServer.config
    readonly property bool backgroundAllowed: effectConfig?.backgroundEffectEnabled ?? true
    property int blurMax: effectConfig?.blurStrength ?? 0
    property bool blurEnabled: backgroundAllowed && (effectConfig?.blurStrength ?? 0) > 0
        && blurMax > 0 && blurAmount > 0
    property real blurAmount: effectConfig?.blurAmount ?? 0
    property real multiplier: effectConfig?.blurMultiplier ?? 0
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: effectConfig?.glassSaturation ?? 0
    property real glassSpecular: effectConfig?.glassSpecular ?? 0
    property real glassTint: 0.0
    property bool glassEnabled: effectConfig?.glassEnabled ?? false
    property Item effectContent: contentLoader
    property bool contentOffscreen: false
    property bool effectEnabled: true

    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent

    // Dispatch between Liquid Glass and traditional blur via a Loader so only
    // the active branch is instantiated.  Toggling the DConfig key unloads one
    // Component and loads the other.
    Loader {
        id: contentLoader
        anchors.fill: parent
        active: blitter.backgroundAllowed
        sourceComponent: blitter.glassEnabled ? glassComponent : blurComponent
        visible: blitter.effectEnabled && !blitter.contentOffscreen
    }

    Component {
        id: glassComponent
        GlassEffect {
            anchors.fill: parent
            source: blitter.content
            radius: blitter.radiusEnabled ? blitter.radius : 0
            frameRect: blitter.frameRect
            blurEnabled: blitter.blurEnabled
            blurMax: blitter.blurMax
            blurAmount: blitter.blurAmount
            blurMultiplier: blitter.multiplier
            bezelWidth: blitter.effectConfig.glassBezel
            thickness: blitter.effectConfig.glassThickness
            ior: 1.33
            specular: blitter.glassSpecular
            tint: blitter.glassTint
            brightness: blitter.brightness
            contrast: blitter.contrast
            saturation: blitter.saturation
            contentEdgePull: 0.0
            contentRampEnd: 0.0
            refractionMaxTan: 3.3
            profilePower: blitter.effectConfig.glassProfilePower
            innerShadow: 0.0
        }
    }

    Component {
        id: blurComponent
        Item {
            anchors.fill: parent

            MultiEffect {
                id: blur
                anchors.fill: parent
                layer.enabled: blitter.radiusEnabled
                smooth: blitter.radiusEnabled
                opacity: blitter.radiusEnabled ? 0 : blitter.opacity
                source: blitter.content
                autoPaddingEnabled: false
                blurEnabled: blitter.blurEnabled
                blur: blitter.blurAmount
                blurMax: blitter.blurMax
                blurMultiplier: blitter.multiplier
                saturation: 0.2
            }

            Loader {
                anchors.fill: parent
                active: blitter.radiusEnabled
                sourceComponent: FrameClip {
                    source: blur
                    geometry: ({ rect: blitter.frameRect, radius: blitter.radius })
                }
            }
        }
    }
}
