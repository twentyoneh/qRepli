import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property bool toolbarItem: false
    property string hint: ""
    implicitHeight: 36
    implicitWidth: Math.max(36, contentItem.implicitWidth + (toolbarItem ? 12 : 24))
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: hint.length ? hint : text
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.text: hint
    ToolTip.delay: 600
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? "#8a8e9a" : control.accent ? "#ffffff" : "#eef0f6"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: 140 } }
    }
    background: Rectangle {
        radius: height / 2
        color: !control.enabled ? "transparent" : control.accent
               ? (control.down ? "#2969bc" : control.hovered ? "#448ce5" : "#337bd3")
               : (control.down || control.checked ? "#35ffffff" : control.hovered ? "#20ffffff"
                  : control.toolbarItem ? "transparent" : "#15ffffff")
        border.color: control.visualFocus ? "#93c9ff" : "#24ffffff"
        border.width: control.visualFocus ? 2 : (control.toolbarItem || !control.enabled ? 0 : 1)
        scale: control.down ? 0.94 : 1
        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    }
}
