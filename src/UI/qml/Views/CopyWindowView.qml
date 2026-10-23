import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: view
    signal addFiles()
    signal addFolder()
    signal chooseDestination()
    signal showParameters()
    signal saveScript()
    signal showHistory()
    signal toggleDetails()
    property bool detailsExpanded: false
    property bool transferOnly: false
    property int completionDefault: 1
    property int completionChoice: completion.currentIndex
    property string actionName: Number(appState.profile.transferMode)===5 ? "Mirror" : [3,4].indexOf(Number(appState.profile.transferMode))>=0 ? "Move" : "Copy"
    function sourceSummary() {
        if (!appState.sources.length) return "Add files or folders"
        const first = appState.sources[0].split(/[\\/]/).pop()
        return appState.sources.length === 1 ? first : first + " + " + (appState.sources.length-1) + " more"
    }
    ColumnLayout {
        objectName: "transferLayout"
        anchors.fill: parent; spacing: 10
        RowLayout {
            Layout.fillWidth: true; Layout.maximumHeight: 28
            Text {text: appState.busy ? appState.paused ? "Transfer paused" : (appState.deleting ? "Deleting items" : "Copying files") : appState.outcome === "success" ? "All done" : appState.outcome === "error" ? "Needs attention" : "Ready to copy";font.pixelSize: 21;font.weight: Font.DemiBold;color: "#ecf0f7";Layout.fillWidth: true}
            Text {text: appState.sourceFileCount ? appState.sourceFileCount + " files · " + appState.totalSize : appState.sources.length ? appState.sources.length + " source" + (appState.sources.length===1 ? "" : "s") : "";font.pixelSize: 11;color: "#8a96a9"}
        }
        Rectangle {
            objectName: "sourcePanel"
            Layout.fillWidth: true; Layout.preferredHeight: 88; Layout.maximumHeight: 88
            radius: 7; color: "#191e27"; border.color: "#2d3542"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; Layout.maximumHeight: 30; spacing: 8
                    Text {text: "From";color: "#8c98aa";font.pixelSize: 12;Layout.preferredWidth: 36}
                    Text {text: view.sourceSummary();color: appState.sources.length ? "#dde5f1" : "#8995a7";font.pixelSize: 12;elide: Text.ElideMiddle;Layout.fillWidth: true;ToolTip.visible: sourceHover.hovered && appState.sources.length>0;ToolTip.text: appState.sources.join("\n");HoverHandler {id: sourceHover}}
                    GlassButton {text: "+ Files";implicitHeight: 28;onClicked: view.addFiles()}
                    GlassButton {text: "+ Folder";implicitHeight: 28;onClicked: view.addFolder()}
                    GlassButton {text: "...";implicitWidth: 29;implicitHeight: 28;onClicked: queueDialog.open();ToolTip.visible: hovered;ToolTip.text: "Manage sources"}
                }
                RowLayout {
                    visible: !appState.deleting
                    Layout.fillWidth: true; Layout.maximumHeight: 30; spacing: 8
                    Text {text: "To";color: "#8c98aa";font.pixelSize: 12;Layout.preferredWidth: 36}
                    GlassField {id: destination;Layout.fillWidth: true;implicitHeight: 29;text: appState.destination;placeholderText: "Choose destination folder";enabled: !appState.busy;onEditingFinished: appState.destination=text}
                    GlassButton {text: "Browse…";implicitHeight: 29;enabled: !appState.busy;onClicked: view.chooseDestination()}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 9
            RowLayout {
                Layout.fillWidth: true; Layout.maximumHeight: 25
                Text {text: Math.round(appState.progress*100)+"%";font.pixelSize: 23;font.weight: Font.DemiBold;color: "#ecf0f7"}
                Text {text: appState.busy ? "estimated" : "complete";visible: appState.busy || appState.progress>0;font.pixelSize: 11;color: "#8693a7";Layout.alignment: Qt.AlignBottom;Layout.bottomMargin: 3}
                Item {Layout.fillWidth: true}
                Text {visible: !appState.deleting;text: appState.paused ? "Paused" : Number(appState.stats.writeMbps || 0).toFixed(1)+" MB/s";font.pixelSize: 13;color: "#a9cfff"}
            }
            Rectangle {
                Layout.fillWidth: true; height: 7; radius: 3; color: "#292f3b"
                Rectangle {height: parent.height;width: parent.width*appState.progress;radius: 3;color: appState.outcome==="error" ? "#ed8495" : appState.outcome==="success" ? "#70c9a8" : "#7bb4ff";Behavior on width {NumberAnimation {duration: 160}}}
            }
            RowLayout {
                Layout.fillWidth: true; Layout.maximumHeight: 16
                Text {text: appState.busy ? appState.phase : appState.sources.length ? "Start when you're ready." : "Drop files here to get started.";color: "#8997ac";font.pixelSize: 11;elide: Text.ElideRight;Layout.fillWidth: true}
                Text {text: "Time left: " + appState.eta;color: "#8997ac";font.pixelSize: 11}
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 60; Layout.maximumHeight: 60
            color: "#151b25"; radius: 5; border.color: "#2b3544"
            visible: !appState.deleting
            TelemetryGraph {anchors.fill: parent;anchors.margins: 8}
            Text {anchors.right: parent.right;anchors.top: parent.top;anchors.margins: 9;text: "Speed: " + Number(appState.stats.writeMbps || 0).toFixed(1) + " MB/s";color: "#778aa5";font.pixelSize: 10}
        }
        ColumnLayout {
            visible: view.detailsExpanded
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 7
            RowLayout {
                Layout.fillWidth: true; Layout.maximumHeight: 30
                ModernComboBox {Layout.preferredWidth: 135;implicitHeight: 29;model: ["All files", "Copied / copying", "Skipped", "Extra items", "Failed", "Verified", "Planned"];onActivated: appState.setFilter(["","Copied","Skipped","Extra","Failed","Verified","Planned"][currentIndex])}
                GlassField {Layout.fillWidth: true;implicitHeight: 29;placeholderText: "Search files";onTextChanged: appState.setSearch(text)}
                GlassButton {text: "Log";implicitHeight: 29;enabled: appState.logPath.length>0;onClicked: appState.openLog()}
            }
            FileTable {Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: 115}
            RowLayout {
                Layout.fillWidth: true; Layout.maximumHeight: 30
                Text {text: "After completion";font.pixelSize: 11;color: "#8c98aa"}
                ModernComboBox {id: completion;Layout.preferredWidth: 150;implicitHeight: 28;currentIndex: view.completionDefault;model: ["Keep open","Close window","Sleep PC…","Eject drive…"]}
                Item {Layout.fillWidth: true}
                GlassButton {text: "History";implicitHeight: 28;onClicked: view.showHistory()}
            }
        }
        ListView {
            visible: !appState.busy && appState.recentRuns.length > 0
            Layout.fillWidth: true;Layout.preferredHeight: 48;clip: true
            model: appState.recentRuns.slice(0, 2)
            delegate: Text {
                required property var modelData
                width: ListView.view.width;height: 24;elide: Text.ElideMiddle
                text: modelData.state + " · " + (modelData.payload.destination || "")
                color: "#92a4ba";font.pixelSize: 11
            }
        }
        Item {visible: !view.detailsExpanded;Layout.fillHeight: true;Layout.minimumHeight: 0}
        Text {
            Layout.fillWidth: true; Layout.maximumHeight: 32
            text: appState.message || (appState.omittedRows>0 || appState.fileStats.omittedCount>0 ? "Activity view capped. Full output is saved in the log." : "Folders keep their name at the destination.")
            font.pixelSize: 10;color: appState.outcome==="error" ? "#ed8495" : "#758399"
            elide: Text.ElideRight;maximumLineCount: 2;wrapMode: Text.WordWrap
            ToolTip.visible: messageHover.hovered;ToolTip.text: text;HoverHandler {id: messageHover}
        }
        Rectangle {Layout.fillWidth: true;height: 1;color: "#2c333f"}
        RowLayout {
            Layout.fillWidth: true; Layout.maximumHeight: 34; spacing: 8
            GlassButton {text: view.detailsExpanded ? "Less" : "Details";onClicked: view.toggleDetails()}
            GlassButton {text: "Settings";onClicked: view.showParameters()}
            GlassButton {text: "History";onClicked: view.showHistory()}
            Item {Layout.fillWidth: true}
            GlassButton {text: "Cancel";visible: appState.busy;danger: true;onClicked: appState.cancel()}
            GlassButton {visible: (appState.busy && !appState.deleting) || (!appState.busy && !view.transferOnly);text: appState.busy ? (appState.paused ? "Resume" : "Pause") : view.actionName;accent: true;Layout.preferredWidth: 90;onClicked: {destination.focus=false;if(appState.busy)appState.togglePause();else {appState.destination=destination.text;appState.start()}}}
        }
    }
    ModernDialog {
        id: queueDialog;title: "Source files and folders"
        width: Math.min(550,view.width+10);height: Math.min(330,view.height+30)
        standardButtons: Dialog.Close
        contentItem: ListView {
            model: appState.sources;clip: true;spacing: 5
            ScrollBar.vertical: SlimScrollBar {}
            delegate: RowLayout {
                required property string modelData
                required property int index
                width: ListView.view.width;height: 35
                Text {text: modelData;color: "#c5cfde";font.pixelSize: 12;elide: Text.ElideMiddle;Layout.fillWidth: true}
                GlassButton {text: "Remove";implicitHeight: 27;enabled: !appState.busy;onClicked: appState.removeSource(index)}
            }
            Text {visible: appState.sources.length===0;anchors.centerIn: parent;text: "No sources added yet.";font.pixelSize: 12;color: "#8d9aad"}
        }
    }
}
