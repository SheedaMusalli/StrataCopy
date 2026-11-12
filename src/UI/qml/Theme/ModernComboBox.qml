import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 32
    implicitWidth: 200
    leftPadding: 9;rightPadding: 28
    font.family: LiquidGlassTheme.font;font.pixelSize: 12;hoverEnabled: true;opacity: enabled ? 1 : .55
    contentItem: Text {text: control.displayText;font: control.font;color: "#f2f2f2";elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
    indicator: Canvas {x: control.width-width-10;y: (control.height-height)/2;width: 8;height: 5;onPaint: {const ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(8,0);ctx.lineTo(4,5);ctx.closePath();ctx.fillStyle="#d0d0d0";ctx.fill()}}
    background: Rectangle {radius: 2;color: control.hovered ? "#3b3b3b" : "#333333";border.color: control.activeFocus ? "#65ade8" : "#555555";border.width: control.activeFocus ? 2 : 1}
    delegate: ItemDelegate {
        id: option
        required property int index
        width: control.popup.width-4;height: 30
        highlighted: control.highlightedIndex===index
        contentItem: Text {text: control.textAt(option.index);font: control.font;color: "#f2f2f2";elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
        background: Rectangle {color: option.highlighted ? "#414141" : option.hovered ? "#383838" : "#303030"}
    }
    popup: Popup {
        margins: 2;y: control.height+2;width: control.width;padding: 2
        implicitHeight: Math.min(contentItem.implicitHeight+4,250)
        background: Rectangle {color: "#303030";border.color: "#555555"}
        contentItem: ListView {clip: true;implicitHeight: contentHeight;model: control.popup.visible ? control.delegateModel : null;currentIndex: control.highlightedIndex;ScrollBar.vertical: SlimScrollBar {}}
    }
}
