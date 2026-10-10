import QtQuick 2.15

Rectangle {
    width: 320
    height: 200
    color: fluentTheme.background
    property string observedTitle: bridgeModel.title
    property int observedProgress: bridgeModel.progress
    property color observedAccent: fluentTheme.accent
    property color observedSurfaceAlt: fluentTheme.surfaceAlt
    function advance() { bridgeModel.progress += 10 }
    function rename() { bridgeModel.title = "Changed in QML" }
    function stopBridge() { bridgeModel.stopBridge() }
}
