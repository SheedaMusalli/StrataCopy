import QtQuick
import QtQuick.Effects
Item {
    id: card
    property real radius: 18
    property Item backdrop: null
    property alias content: contentItem.data
    default property alias contents: contentItem.data
    clip: true
    Rectangle {anchors.fill: parent;radius: card.radius;color: "#121b29";border.color: "#2e3c50";visible: GraphicsInfo.api===GraphicsInfo.Software}
    ShaderEffectSource {
        id: capture
        visible: false
        sourceItem: card.backdrop
        sourceRect: card.backdrop ? Qt.rect(card.mapToItem(card.backdrop,0,0).x,card.mapToItem(card.backdrop,0,0).y,card.width,card.height) : Qt.rect(0,0,1,1)
        live: true
    }
    MultiEffect {
        anchors.fill: parent
        visible: card.backdrop !== null
        source: capture
        blurEnabled: true
        blurMax: 48
        blur: 1
        saturation: 0.35
        maskEnabled: true
        maskSource: mask
    }
    Rectangle { id: mask; anchors.fill: parent; radius: card.radius; layer.enabled: true; visible: false; color: "white" }
    ShaderEffect {
        anchors.fill: parent
        property vector2d resolution: Qt.vector2d(width,height)
        property real radius: card.radius
        fragmentShader: "qrc:/shaders/liquid_glass_card.frag.qsb"
    }
    Item { id: contentItem; anchors.fill: parent }
}
