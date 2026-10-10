import QtQuick 2.15
Item {
    property int observedCount: list.count
    ListView {
        id: list
        anchors.fill: parent
        model: listModel
        delegate: Text { text: display }
    }
}
