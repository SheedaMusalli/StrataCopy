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
        color: "#252525"; border.color: control.checked ? "#62b6ec" : "#8a8a8a"
        Rectangle {anchors.centerIn: parent;width: control.checked ? 9 : 0;height: width;radius: 5;color: "#62b6ec";Behavior on width {NumberAnimation {duration: 100}}}
    }
    contentItem: Text {leftPadding: 29;rightPadding: 100;text: control.text;color: "#eeeeee";font: control.font;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    Text {anchors.right: parent.right;anchors.rightMargin: 8;anchors.verticalCenter: parent.verticalCenter;text: control.badge;color: "#bdbdbd";font.family: LiquidGlassTheme.mono;font.pixelSize: 10}
}
