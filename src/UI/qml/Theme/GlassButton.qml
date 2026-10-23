import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    property bool danger: false
    implicitHeight: 34
    implicitWidth: Math.max(38,contentItem.implicitWidth+28)
    font.family: LiquidGlassTheme.font
    font.pixelSize: 12
    font.weight: Font.DemiBold
    hoverEnabled: true
    opacity: enabled ? 1 : .35
    contentItem: Text { text: control.text; font: control.font; color: control.accent ? "#061318" : control.danger ? LiquidGlassTheme.red : LiquidGlassTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    background: Rectangle {
        radius: 6
        color: control.accent ? (control.down ? "#5993db" : control.hovered ? "#9ac5ff" : "#7bb4ff") : (control.down ? "#2e3541" : control.hovered ? "#2b323e" : "#202630")
        border.color: control.accent ? "#9bc8ff" : control.danger ? "#633447" : "#38414f"
        border.width: 1
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
