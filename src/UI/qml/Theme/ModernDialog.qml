import QtQuick
import QtQuick.Controls
Dialog {
    id: dialog
    property var enterAction: null
    modal: true;anchors.centerIn: parent;padding: 14
    function confirmFromEnter() {
        if (!opened || !activeFocus) return
        if (enterAction) {enterAction();return}
        if (standardButtons & (Dialog.Ok | Dialog.Yes | Dialog.Save | Dialog.Open | Dialog.Apply | Dialog.Retry | Dialog.Close)) accept()
    }
    Shortcut {sequence: "Return";context: Qt.WindowShortcut;enabled: dialog.opened&&dialog.activeFocus;onActivated: dialog.confirmFromEnter()}
    Shortcut {sequence: "Enter";context: Qt.WindowShortcut;enabled: dialog.opened&&dialog.activeFocus;onActivated: dialog.confirmFromEnter()}
    background: Rectangle {color: "#2c2c2c";border.color: "#555555";border.width: 1}
    header: Text {text: dialog.title;leftPadding: 10;rightPadding: 10;topPadding: 10;bottomPadding: 8;color: "#f2f2f2";font.family: LiquidGlassTheme.font;font.pixelSize: 15;font.weight: Font.Medium}
    footer: DialogButtonBox {
        visible: dialog.standardButtons!==Dialog.NoButton;standardButtons: dialog.standardButtons;padding: 10;spacing: 8
        background: Rectangle {color: "#2c2c2c"}
        delegate: GlassButton {}
        onAccepted: dialog.accept();onRejected: dialog.reject()
    }
}
