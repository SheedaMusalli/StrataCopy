import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
ApplicationWindow {
    id: root
    width: 640; height: 510
    minimumWidth: 600; minimumHeight: detailsExpanded ? 650 : 510
    visible: !transferOnly; title: "StrataCopy"; color: "#12161d"
    flags: Qt.Window | Qt.FramelessWindowHint | (pinned ? Qt.WindowStaysOnTopHint : 0)
    property bool detailsExpanded: uiPreferences.detailed
    Settings {id: uiPreferences;category: "Interface";property bool detailed: false;property bool pinned: false;property int afterCopy: 1;property bool transferOnly: false;property bool completionNotifications: true;property bool notificationAnimations: true}
    property bool pinned: uiPreferences.pinned
    property alias completionNotifications: uiPreferences.completionNotifications
    property alias notificationAnimations: uiPreferences.notificationAnimations
    property alias transferOnly: uiPreferences.transferOnly
    property bool allowClose: false
    property bool permanentDeletion: false
    property var deletionPaths: []
    property string confirmationText: ""
    property string updateMessage: ""
    property var currentConflict: ({})
    property int pendingCompletion: 0
    font.family: LiquidGlassTheme.font
    palette.window: "#191d25"; palette.windowText: "#d5dce8"; palette.base: "#141922"
    palette.text: "#d5dce8"; palette.button: "#252c37"; palette.buttonText: "#d5dce8"
    palette.highlight: "#2c4160"; palette.highlightedText: "#f0f5ff"
    onDetailsExpandedChanged: height=detailsExpanded ? 670 : 510
    onClosing: function(event) {if(appState.busy&&!allowClose){event.accepted=false;closeDialog.open()}else if(root.transferOnly){event.accepted=false;root.hide()}}
    function setUiTestState(mode) {
        parameters.close();runHistory.close()
        detailsExpanded=mode==="details"
        if(mode==="parameters" || mode==="presets") {
            parameters.open()
            if(mode==="presets") Qt.callLater(parameters.openPresetPopup)
        }
        if(mode==="history")runHistory.open()
    }
    Rectangle {anchors.fill: parent;color: "transparent";border.color: "#333c4b";radius: 10}
    ColumnLayout {
        objectName: "windowLayout"
        anchors.fill: parent;anchors.margins: 20;anchors.topMargin: 10;spacing: 17
        RowLayout {
            Layout.fillWidth: true;Layout.preferredHeight: 26;Layout.maximumHeight: 26;spacing: 8
            Image {source: "qrc:/resources/icons/stratacopy.svg";sourceSize: Qt.size(48,48);Layout.preferredWidth: 23;Layout.preferredHeight: 23}
            Text {text: "StrataCopy";font.pixelSize: 13;font.weight: Font.DemiBold;color: "#dce5f3"}
            Item {Layout.fillWidth: true;Layout.fillHeight: true;MouseArea {anchors.fill: parent;onPressed: root.startSystemMove()}}
            GlassButton {text: "Pin";accent: root.pinned;implicitWidth: 35;implicitHeight: 25;onClicked: root.pinned=!root.pinned;ToolTip.visible: hovered;ToolTip.text: "Keep on top"}
            GlassButton {text: "−";implicitWidth: 29;implicitHeight: 25;onClicked: root.showMinimized()}
            GlassButton {text: "×";implicitWidth: 29;implicitHeight: 25;onClicked: root.close()}
        }
        CopyWindowView {
            id: copyView
            objectName: "copyView"
            Layout.fillWidth: true;Layout.fillHeight: true
            detailsExpanded: root.detailsExpanded
            completionDefault: uiPreferences.afterCopy
            transferOnly: root.transferOnly
            onToggleDetails: root.detailsExpanded=!root.detailsExpanded
            onAddFiles: sourceFiles.open()
            onAddFolder: sourceFolder.open()
            onChooseDestination: destinationFolder.open()
            onShowParameters: parameters.open()
            onSaveScript: scriptFile.open()
            onShowHistory: runHistory.open()
        }
    }
    DropArea {
        anchors.fill: parent
        onDropped: function(drop){if(drop.hasUrls){appState.addSources(drop.urls);drop.acceptProposedAction()}}
        Rectangle {anchors.fill: parent;visible: parent.containsDrag;color: "#e619273b";border.color: "#7bb4ff";border.width: 2;radius: 10;Text {anchors.centerIn: parent;text: appState.busy ? "Drop to queue the next transfer" : "Drop to add files";font.pixelSize: 20;color: "#c3dcff"}}
    }
    ParameterMatrixDrawer {id: parameters;preferences: uiPreferences;onMoveWindowRequested: root.startSystemMove()}
    FileDialog {id: sourceFiles;title: "Add source files";fileMode: FileDialog.OpenFiles;onAccepted: appState.addSources(selectedFiles)}
    FolderDialog {id: sourceFolder;title: "Add source folder";onAccepted: appState.addSources([selectedFolder])}
    FolderDialog {id: destinationFolder;title: "Choose destination";onAccepted: appState.destination=appState.pathFromUrl(selectedFolder)}
    FileDialog {id: scriptFile;title: "Export Robocopy script";fileMode: FileDialog.SaveFile;nameFilters: ["PowerShell script (*.ps1)","Batch script (*.bat)"];defaultSuffix: selectedNameFilter.index===0 ? "ps1" : "bat";onAccepted: appState.exportScript(selectedFile)}
    ModernDialog {
        id: destructiveDialog;title: "Confirm operation";width: root.width-36;height: Math.min(370,root.height-36)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            contentWidth: availableWidth
            ScrollBar.vertical: SlimScrollBar {}
            TextArea {text: root.confirmationText;readOnly: true;wrapMode: TextEdit.WrapAnywhere;color: "#ddc1a4";font.pixelSize: 12;background: null}
        }
        onAccepted: appState.start(false,true)
        onRejected: appState.rejectExplorerRequest()
    }
    ModernDialog {
        id: deletionDialog;title: root.permanentDeletion ? "Permanently delete these items?" : "Move these items to the Recycle Bin?"
        width: Math.min(410,root.width-36);height: Math.min(300,root.height-36);padding: 12;standardButtons: Dialog.Cancel
        enterAction: function() {deletionDialog.accept()}
        contentItem: ColumnLayout {
            spacing: 8
            Text {text: root.deletionPaths.length+" selected item(s)";color: "#e6edf5";font.pixelSize: 15}
            Text {text: root.permanentDeletion ? "Permanent deletion cannot be undone." : "Windows may ask if recycling is unavailable.";Layout.fillWidth: true;wrapMode: Text.WordWrap;color: "#b5c3d5";font.pixelSize: 12}
            ScrollView {
                Layout.fillWidth: true;Layout.preferredHeight: 72;Layout.minimumHeight: 56;Layout.maximumHeight: 72;contentWidth: availableWidth;clip: true
                ScrollBar.vertical: SlimScrollBar {}
                TextArea {text: root.deletionPaths.slice(0,8).join("\n")+(root.deletionPaths.length>8 ? "\n…" : "");readOnly: true;selectByMouse: true;wrapMode: TextEdit.WrapAnywhere;color: "#95a9be";font.pixelSize: 11;background: null}
            }
            GlassButton {text: root.permanentDeletion ? "Delete permanently" : "Move to Recycle Bin";danger: root.permanentDeletion;accent: !root.permanentDeletion;onClicked: deletionDialog.accept()}
        }
        onAccepted: appState.confirmDeletion()
        onRejected: {appState.rejectExplorerRequest();if(root.transferOnly)root.hide()}
    }
    ModernDialog {
        id: closeDialog;title: "Cancel this transfer?";width: 390;standardButtons: Dialog.Yes | Dialog.No
        contentItem: Text {text: appState.deleting ? "Items already removed are not restored by cancellation." : "Completed files will remain at the destination.";color: "#abb9cc";wrapMode: Text.WordWrap;font.pixelSize: 12}
        onAccepted: {appState.cancel();if(appState.deleting)root.hide();else {root.allowClose=true;root.close()}}
    }
    ModernDialog {
        id: completionDialog;title: root.pendingCompletion===2 ? "Put the PC to sleep?" : "Eject the drive?";width: 400;standardButtons: Dialog.Yes | Dialog.No
        contentItem: Text {text: root.pendingCompletion===2 ? "The transfer is complete. Sleep now?" : "The transfer is complete. Eject the removable destination drive?";color: "#abb9cc";wrapMode: Text.WordWrap;font.pixelSize: 12}
        onAccepted: appState.completionAction(root.pendingCompletion)
    }
    ModernDialog {
        id: runHistory;title: "Recent transfers";width: root.width-36;height: Math.min(380,root.height-36);standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            RowLayout {Layout.fillWidth: true;Text {text: "Restore a queue to review and retry.";font.pixelSize: 11;color: "#8a9bb3";Layout.fillWidth: true}GlassButton {text: "Explorer menu";implicitHeight: 28;onClicked: shellDialog.open()}}
            ListView {
                Layout.fillWidth: true;Layout.fillHeight: true;clip: true;spacing: 5;model: appState.recentRuns
                ScrollBar.vertical: SlimScrollBar {}
                delegate: Rectangle {
                    required property var modelData;required property int index
                    width: ListView.view.width;height: 73;radius: 5;color: "#212834"
                    RowLayout {anchors.fill: parent;anchors.margins: 9;ColumnLayout {Layout.fillWidth: true;spacing: 3;Text {text: modelData.started+" UTC · "+modelData.state;color: "#d2deed";font.pixelSize: 11}Text {text: modelData.message || "In progress";Layout.fillWidth: true;elide: Text.ElideRight;color: "#8b9bb1";font.pixelSize: 10}Text {text: modelData.payload.destination || "";Layout.fillWidth: true;elide: Text.ElideMiddle;color: "#7d9ec5";font.pixelSize: 10}}GlassButton {text: "Restore";implicitHeight: 28;enabled: !appState.busy;onClicked: {appState.restoreRun(index);runHistory.close()}}}
                }
                Text {anchors.centerIn: parent;visible: parent.count===0;text: "No recent transfers.";color: "#8698b0";font.pixelSize: 12}
            }
        }
    }
    ModernDialog {
        id: shellDialog;title: "Explorer integration";width: 460;standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Text {text: "Right-click Copy / Paste with StrataCopy (Show more options on Windows 11).";color: "#abb9cc";wrapMode: Text.WordWrap;Layout.fillWidth: true;font.pixelSize: 12}
            RowLayout {GlassButton {text: "Add menu actions";onClicked: appState.setShellIntegration(true)}GlassButton {text: "Remove actions";onClicked: appState.setShellIntegration(false)}}
            Text {text: "Use StrataCopy for Explorer file copy/move: keyboard paste, native Paste and drag-and-drop. Loads a native hook into Explorer. Virtual items and operations Robocopy cannot represent use Windows. Disabling takes effect immediately; the DLL unloads when Explorer exits.";color: "#abb9cc";wrapMode: Text.WordWrap;Layout.fillWidth: true;font.pixelSize: 12}
            RowLayout {GlassButton {text: "Use as Explorer copier";onClicked: appState.setKeyboardPasteIntegration(true)}GlassButton {text: "Disable";onClicked: appState.setKeyboardPasteIntegration(false)}}
        }
    }
    ModernDialog {
        id: updateDialog;title: "Check for Updates";width: Math.min(450,root.width-36)
        standardButtons: Dialog.Ok
        contentItem: Text {text: root.updateMessage;color: "#c5cfde";font.pixelSize: 12;wrapMode: Text.WordWrap}
    }
    ModernDialog {
        id: fileConflictDialog;title: "File already exists";width: root.width-36
        standardButtons: Dialog.Cancel
        enterAction: function() {appState.resolveFileConflict(root.currentConflict.token,0,allConflicts.checked);fileConflictDialog.close()}
        contentItem: ColumnLayout {
            spacing: 12
            Text {text: root.currentConflict.name || "";color: "#edf2fa";font.pixelSize: 16;font.weight: Font.DemiBold;Layout.fillWidth: true;elide: Text.ElideMiddle}
            Text {text: "Choose what to do with this file.";color: "#9babbe";font.pixelSize: 12}
            Text {text: "Incoming: " + (root.currentConflict.source || "") + "\n" + (root.currentConflict.sourceSize || "") + " · " + (root.currentConflict.sourceDate || "");color: "#bfcddd";font.pixelSize: 11;wrapMode: Text.WrapAnywhere;Layout.fillWidth: true}
            Text {text: "Existing: " + (root.currentConflict.destination || "") + "\n" + (root.currentConflict.destinationSize || "") + " · " + (root.currentConflict.destinationDate || "");color: "#bfcddd";font.pixelSize: 11;wrapMode: Text.WrapAnywhere;Layout.fillWidth: true}
            LiquidCheckBox {id: allConflicts;text: "Do this for all conflicts"}
            RowLayout {
                Layout.fillWidth: true;spacing: 8
                GlassButton {text: "Replace Existing File";accent: true;Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,0,allConflicts.checked);fileConflictDialog.close()}}
                GlassButton {text: "Skip This File";Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,1,allConflicts.checked);fileConflictDialog.close()}}
                GlassButton {text: "Make Duplicate";Layout.fillWidth: true;onClicked: {appState.resolveFileConflict(root.currentConflict.token,2,allConflicts.checked);fileConflictDialog.close()}}
            }
        }
        onRejected: appState.resolveFileConflict(root.currentConflict.token,3)
    }
    DryRunDiffModal {id: preview}
    Connections {
        target: appState
        function onUpdateCheckFinished(message){root.updateMessage=message;updateDialog.open()}
        function onDeletionConfirmationRequired(permanent,paths){root.permanentDeletion=permanent;root.deletionPaths=paths;root.showNormal();root.raise();root.requestActivate();deletionDialog.open()}
        function onFileConflictRequired(conflict){root.currentConflict=conflict;allConflicts.checked=false;fileConflictDialog.open()}
        function onDestructiveConfirmationRequired(message){root.confirmationText=message;destructiveDialog.open()}
        function onCloseRequested(){root.close()}
        function onPreviewReady(){root.detailsExpanded=true}
        function onCompletedSuccessfully(){if(root.transferOnly)root.hide();else if(copyView.completionChoice===1)appState.completionAction(1);else if(copyView.completionChoice>1 && !appState.deleting){root.pendingCompletion=copyView.completionChoice;completionDialog.open()}}
    }
    MouseArea {anchors.right: parent.right;anchors.bottom: parent.bottom;width: 9;height: 9;cursorShape: Qt.SizeFDiagCursor;onPressed: root.startSystemResize(Qt.RightEdge|Qt.BottomEdge)}
}
