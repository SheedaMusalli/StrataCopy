import QtQuick
Item {
    id: bar
    property real progress: 0
    property bool active: false
    implicitHeight: 9
    Rectangle {anchors.fill: parent;radius: height/2;color: "#0a111d";border.color: "#253347"}
    Rectangle {height: parent.height;width: parent.width*bar.progress;radius: height/2;color: LiquidGlassTheme.cyan;visible: GraphicsInfo.api===GraphicsInfo.Software}
    ShaderEffect {
        anchors.fill: parent
        property real time: 0
        property real progress: bar.progress
        property vector2d resolution: Qt.vector2d(width,height)
        fragmentShader: "qrc:/shaders/liquid_plasma_bar.frag.qsb"
        NumberAnimation on time {from: 0;to: 1000;duration: 1000000;loops: Animation.Infinite;running: bar.active}
    }
}
