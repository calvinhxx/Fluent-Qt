import QtQuick 2.15

Rectangle {
    id: preview
    color: fluentTheme.surface
    radius: fluentTheme.cornerRadius
    antialiasing: true
    property int previewProgress: widgetProgress ? Math.round(widgetProgress.value) : 0

    Rectangle {
        anchors.fill: parent
        z: -1
        color: fluentTheme.surfaceAlt
    }

    function advance() {
        if (widgetProgress)
            widgetProgress.value = Math.min(100, widgetProgress.value + 10)
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        Text {
            text: preview.previewProgress + "% in Widgets"
            color: fluentTheme.text
            font.family: fluentTheme.fontFamily
            font.pixelSize: fluentTheme.fontPixelSize
        }
        Rectangle {
            width: parent.width
            height: 6
            radius: 3
            antialiasing: true
            color: fluentTheme.border
            Rectangle {
                width: parent.width * preview.previewProgress / 100
                height: parent.height
                radius: parent.radius
                antialiasing: true
                color: fluentTheme.accent
            }
        }
        Rectangle {
            id: action
            objectName: "qmlBridgeAdvanceButton"
            width: Math.min(parent.width, 160)
            height: 36
            radius: fluentTheme.cornerRadius
            antialiasing: true
            color: fluentTheme.accent
            activeFocusOnTab: true
            border.width: activeFocus ? 2 : 0
            border.color: fluentTheme.text
            Accessible.role: Accessible.Button
            Accessible.name: "Advance +10"
            Accessible.onPressAction: preview.advance()
            Keys.onReturnPressed: preview.advance()
            Keys.onEnterPressed: preview.advance()
            Keys.onSpacePressed: preview.advance()
            Text {
                anchors.centerIn: parent
                text: "Advance +10"
                color: fluentTheme.onAccent
                font.family: fluentTheme.fontFamily
                font.pixelSize: fluentTheme.fontPixelSize
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    action.forceActiveFocus()
                    preview.advance()
                }
            }
        }
    }
}
