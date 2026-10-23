import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 34
    implicitWidth: 200
    leftPadding: 11; rightPadding: 30
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    contentItem: Text {
        text: control.displayText; font: control.font
        color: "#dce2ec"; elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Canvas {
        x: control.width - width - 12; y: (control.height-height)/2
        width: 8; height: 5
        onPaint: {const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(8,0);ctx.lineTo(4,5);ctx.closePath();ctx.fillStyle="#9ba5b5";ctx.fill()}
    }
    background: Rectangle {
        radius: 6; color: control.hovered ? "#252a34" : "#1d222b"
        border.color: control.activeFocus ? "#6aa8ff" : "#343b48"
    }
    delegate: ItemDelegate {
        id: option
        required property int index
        width: control.popup.width - 10
        height: 36
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textAt(option.index); font: control.font
            color: option.highlighted ? "#f0f5ff" : "#c4ccda"
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: option.highlighted ? "#2c4160" : option.hovered ? "#272e39" : "transparent"
        }
    }
    popup: Popup {
        margins: 6
        y: control.height + 5
        width: control.width
        padding: 5
        implicitHeight: Math.min(contentItem.implicitHeight + 10, 250)
        background: Rectangle {color: "#1a1f28"; radius: 7; border.color: "#414a5a"}
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollBar.vertical: SlimScrollBar {}
        }
    }
}
