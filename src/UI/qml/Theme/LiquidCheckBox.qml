import QtQuick
import QtQuick.Controls
CheckBox {
    id: control
    property string badge: ""
    implicitHeight: 36
    spacing: 10
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    indicator: Rectangle {
        x: 5;y: (control.height-height)/2;width: 17;height: 17;radius: 5
        color: control.checked ? "#104452" : "#090f19";border.color: control.checked ? "#36bfce" : "#465168"
        Text {anchors.centerIn: parent;text: "✓";font.pixelSize: 12;font.bold: true;color: LiquidGlassTheme.cyan;visible: control.checked}
    }
    contentItem: Text {leftPadding: 29;rightPadding: 100;text: control.text;color: control.checked ? LiquidGlassTheme.text : "#a0adc0";font: control.font;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    Text {anchors.right: parent.right;anchors.rightMargin: 8;anchors.verticalCenter: parent.verticalCenter;text: control.badge;color: "#6b9da9";font.family: LiquidGlassTheme.mono;font.pixelSize: 10}
}
