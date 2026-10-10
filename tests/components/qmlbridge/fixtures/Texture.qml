import QtQuick 2.15

Rectangle {
    width: 320
    height: 200
    color: "#17212b"
    property int localCounter: 7
    property int completedAfterStop: 0
    focus: true
    Keys.onReturnPressed: localCounter += 1
    function stopAndContinue() {
        bridgeModel.stopBridge()
        completedAfterStop += 1
    }
    Rectangle { width: 40; height: 40; color: "#ff0000" }
    Rectangle {
        x: parent.width - width
        y: parent.height - height
        width: 40; height: 40; color: "#0000ff"
    }
    Rectangle {
        x: 48; y: 90
        width: (parent.width - 96) * bridgeModel.progress / 100
        height: 20
        color: "#00ff00"
    }
    MouseArea {
        anchors.fill: parent
        onClicked: {
            parent.forceActiveFocus()
            parent.localCounter += 1
        }
    }
}
