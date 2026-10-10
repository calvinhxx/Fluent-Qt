import QtQuick 2.15

Rectangle {
    color: fluentTheme.surface
    radius: fluentTheme.cornerRadius
    antialiasing: true

    Rectangle {
        anchors.fill: parent
        z: -1
        color: fluentTheme.surfaceAlt
    }

    Text {
        anchors.centerIn: parent
        text: "Hello from QML"
        color: fluentTheme.text
        font.family: fluentTheme.fontFamily
        font.pixelSize: fluentTheme.fontPixelSize
    }
}
