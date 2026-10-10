import QtQuick 2.15

Rectangle {
    color: fluentTheme.surface
    radius: fluentTheme.cornerRadius
    antialiasing: true
    property string previewTitle: widgetInput ? widgetInput.text : ""

    Rectangle {
        anchors.fill: parent
        z: -1
        color: fluentTheme.surfaceAlt
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 32
        horizontalAlignment: Text.AlignHCenter
        text: parent.previewTitle
        elide: Text.ElideRight
        color: fluentTheme.text
        font.family: fluentTheme.fontFamily
        font.pixelSize: fluentTheme.fontPixelSize
    }
}
