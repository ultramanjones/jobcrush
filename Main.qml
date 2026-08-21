import QtQuick

Window {
    id: root

    width: 1280
    height: 800
    visible: true
    title: qsTr("Job Crush")
    color: "#121418"

    Column {
        anchors.centerIn: parent
        spacing: 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Job Crush"
            color: "#E8EAED"
            font.pixelSize: 42
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Phase 1 — skeleton online")
            color: "#7A8290"
            font.pixelSize: 16
        }
    }
}
