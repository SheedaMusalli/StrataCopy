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
        color: control.checked ? "#167cb9" : "#252525";border.color: control.checked ? "#62b6ec" : "#8a8a8a"
        Text {anchors.centerIn: parent;text: "✓";font.pixelSize: 12;font.bold: true;color: "#ffffff";visible: control.checked}
    }
    contentItem: Text {leftPadding: 29;rightPadding: 100;text: control.text;color: "#eeeeee";font: control.font;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    Text {anchors.right: parent.right;anchors.rightMargin: 8;anchors.verticalCenter: parent.verticalCenter;text: control.badge;color: "#bdbdbd";font.family: LiquidGlassTheme.mono;font.pixelSize: 10}
}
