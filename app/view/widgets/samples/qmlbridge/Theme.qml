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

    Row {
        anchors.centerIn: parent
        height: 24
        spacing: 12
        Rectangle {
            objectName: "qmlBridgeThemeSwatch"
            width: 24
            height: 24
            radius: fluentTheme.cornerRadius
            antialiasing: true
            color: fluentTheme.accent
        }
        Text {
            objectName: "qmlBridgeThemeCaption"
            anchors.alignWhenCentered: false
            anchors.verticalCenter: parent.verticalCenter
            text: fluentTheme.dark ? "Dark theme" : "Light theme"
            color: fluentTheme.text
            font.family: fluentTheme.fontFamily
            font.pixelSize: fluentTheme.fontPixelSize
        }
    }
}
