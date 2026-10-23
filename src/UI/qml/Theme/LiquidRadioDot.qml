import QtQuick
import QtQuick.Controls
RadioButton {
    id: control
    property string badge: ""
    implicitHeight: 32
    leftPadding: 4
    spacing: 10
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    indicator: Rectangle {
        x: 5; y: (control.height-height)/2
        width: 17; height: 17; radius: 9
        color: "#090f19"; border.color: control.checked ? "#37dce9" : "#465168"
        Rectangle {anchors.centerIn: parent;width: control.checked ? 9 : 0;height: width;radius: 5;color: LiquidGlassTheme.cyan;Behavior on width {SpringAnimation {spring: 4;damping: .3}}}
    }
    contentItem: Text {leftPadding: 29;rightPadding: 100;text: control.text;color: control.checked ? LiquidGlassTheme.text : "#a0adc0";font: control.font;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    Text {anchors.right: parent.right;anchors.rightMargin: 8;anchors.verticalCenter: parent.verticalCenter;text: control.badge;color: control.checked ? "#62d7e7" : "#6b7a93";font.family: LiquidGlassTheme.mono;font.pixelSize: 10}
}
