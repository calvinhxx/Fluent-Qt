import QtQuick 2.15

Rectangle {
    id: preview
    objectName: "qmlBridgePreview"
    color: fluentTheme.surface
    radius: fluentTheme.cornerRadius
    border.color: fluentTheme.border
    property string previewTitle: previewModel ? previewModel.title : ""
    property int previewProgress: previewModel ? previewModel.progress : 0

    function advance() {
        previewModel.title = "Updated from QML"
        previewModel.progress = Math.min(100, previewModel.progress + 10)
    }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Text {
            text: "QML preview"
            color: fluentTheme.secondaryText
            font.family: fluentTheme.fontFamily
            font.pixelSize: fluentTheme.fontPixelSize
        }
        Text {
            objectName: "qmlBridgePreviewTitle"
            width: parent.width
            text: preview.previewTitle
            color: fluentTheme.text
            font.family: fluentTheme.fontFamily
            font.pixelSize: fluentTheme.fontPixelSize + 6
            font.bold: true
            elide: Text.ElideRight
        }
        Rectangle {
            width: parent.width
            height: 6
            radius: 3
            color: fluentTheme.border
            Rectangle {
                width: parent.width * preview.previewProgress / 100
                height: parent.height
                radius: parent.radius
                color: fluentTheme.accent
                Behavior on width {
                    NumberAnimation { duration: fluentTheme.transitionDuration }
                }
            }
        }
        Text {
            text: preview.previewProgress + "% complete"
            color: fluentTheme.secondaryText
            font.family: fluentTheme.fontFamily
            font.pixelSize: fluentTheme.fontPixelSize
        }
        Rectangle {
            id: action
            objectName: "qmlBridgeAdvanceButton"
            width: Math.min(parent.width, 160)
            height: 36
            radius: fluentTheme.cornerRadius
            color: fluentTheme.accent
            opacity: pointer.pressed ? 0.8 : 1
            border.width: activeFocus ? 2 : 0
            border.color: fluentTheme.text
            activeFocusOnTab: true
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
                id: pointer
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
