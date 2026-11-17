import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property string hint: ""
    implicitHeight: 36
    implicitWidth: Math.max(36, contentItem.implicitWidth + 24)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: hint.length ? hint : text
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.text: hint
    ToolTip.delay: 600
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? "#586174" : control.accent ? "#0a1920" : "#e4e9f2"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 8
        color: control.accent
               ? (control.down ? "#65c9ba" : control.hovered ? "#a9ebd9" : "#87ddc8")
               : (control.down || control.checked ? "#303b50" : control.hovered ? "#283245" : "#1c2433")
        opacity: control.enabled ? 1 : 0.55
        border.color: control.activeFocus ? "#87ddc8" : "#303a4e"
        border.width: control.activeFocus ? 2 : 1
    }
}
