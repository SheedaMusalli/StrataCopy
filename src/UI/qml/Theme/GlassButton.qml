import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    property bool danger: false
    implicitHeight: 32
    implicitWidth: Math.max(40,contentItem.implicitWidth+24)
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    font.weight: Font.Normal
    hoverEnabled: true
    opacity: enabled ? 1 : .55
    contentItem: Text {text: control.text;font: control.font;color: control.accent ? "#101820" : control.danger ? "#ff8792" : "#f1f1f1";horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter}
    background: Rectangle {
        radius: 3
        color: control.accent ? (control.down ? "#4c9bd9" : control.hovered ? "#70b6f0" : "#65ade8") : (control.down ? "#484848" : control.hovered ? "#414141" : "#383838")
        border.color: control.accent ? "#65ade8" : control.danger ? "#8e424a" : "#505050"
        border.width: control.activeFocus ? 2 : 1
    }
}
