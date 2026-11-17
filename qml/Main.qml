import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import qRepli

ApplicationWindow {
    id: window
    required property PlayerController controller
    required property InterfaceLanguage interfaceLanguage
    property bool openPickerOnStartup: true
    property bool showOriginal: true
    property bool showTranslation: true
    property bool showTranscript: false
    width: 1180
    height: 760
    minimumWidth: 620
    minimumHeight: 540
    visible: true
    title: controller.hasMedia ? controller.fileName + " — qRepli" : "qRepli"
    color: "#0b0c10"
    font.family: Qt.platform.os === "osx" ? "Helvetica Neue" : "Segoe UI"
    font.pixelSize: 13
    palette.window: "#17191f"
    palette.windowText: "#e4e9f2"
    palette.text: "#e4e9f2"
    palette.buttonText: "#e4e9f2"
    palette.base: "#20232c"
    palette.button: "#292d37"
    palette.highlight: "#88bdff"
    palette.highlightedText: "#101620"
    palette.mid: "#3b475d"
    palette.dark: "#0b0f16"

    Component.onCompleted: {
        if (openPickerOnStartup) {
            Qt.callLater(() => {
                if (!controller.hasMedia && !videoDialog.visible) videoDialog.open()
            })
        }
    }

    function rateLabel(rate) {
        return (interfaceLanguage.language === "ru" ? String(rate).replace(".", ",") : String(rate)) + "×"
    }

    function toggleFullScreen() {
        if (visibility === Window.FullScreen) showNormal()
        else showFullScreen()
    }

    function toggleRecognition() {
        if (controller.asrRunning) controller.stopTranscription()
        else if (!controller.modelPath.length || !controller.ffmpegPath.length) asrSettings.open()
        else controller.startTranscription()
    }

    component PlayerSlider: Slider {
        id: slider
        hoverEnabled: true
        leftPadding: 6
        rightPadding: 6
        background: Rectangle {
            x: slider.leftPadding
            y: (slider.height - height) / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: "#42ffffff"
            Rectangle {
                width: parent.width * slider.visualPosition
                height: parent.height
                radius: 2
                color: slider.enabled ? "#a6cfff" : "#83909f"
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            implicitWidth: 12
            implicitHeight: 12
            radius: width / 2
            color: slider.enabled ? "#e4e9f2" : "#586174"
            scale: slider.enabled && (slider.hovered || slider.pressed) ? 1.25 : 1
            border.width: slider.visualFocus ? 2 : 0
            border.color: "#88bdff"
            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
    }

    component ControlGroup: Rectangle {
        property color topTint: "#b0383b46"
        property color bottomTint: "#b01c1e26"
        implicitHeight: 44
        radius: height / 2
        border.color: "#35ffffff"
        gradient: Gradient {
            GradientStop { position: 0; color: topTint }
            GradientStop { position: 1; color: bottomTint }
        }
    }

    component VideoControls: ControlGroup {
        opacity: stage.controlsShown ? 1 : 0
        visible: opacity > 0
        enabled: stage.controlsShown
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.InOutQuad } }
    }

    component PlayerMenuItem: MenuItem {
        id: item
        hoverEnabled: true
        leftPadding: 12
        rightPadding: 32
        contentItem: Text {
            text: item.text
            textFormat: Text.PlainText
            font: item.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: item.enabled ? "#eef0f6" : "#9ca2b0"
        }
        indicator: Text {
            visible: item.checkable && item.checked
            x: item.width - width - 12
            y: (item.height - height) / 2
            text: "✓"
            color: item.enabled ? "#93c9ff" : "#9ca2b0"
            font: item.font
        }
        background: Rectangle {
            implicitHeight: 38
            radius: 10
            color: !item.enabled ? "transparent" : item.down ? "#34ffffff"
                   : item.hovered || item.highlighted || item.visualFocus ? "#20ffffff" : "transparent"
            border.width: item.enabled && item.visualFocus ? 1 : 0
            border.color: "#93c9ff"
            Behavior on color { ColorAnimation { duration: 140 } }
        }
    }

    component PlayerMenu: Menu {
        popupType: Popup.Item
        width: 236
        padding: 7
        delegate: PlayerMenuItem {}
        background: Rectangle {
            radius: 16
            color: "#f1252832"
            border.color: "#40ffffff"
        }
    }

    component OpenVideoMenuItem: PlayerMenuItem {
        text: qsTr("Открыть видео…")
        onTriggered: videoDialog.open()
    }

    component RecognitionMenuItem: PlayerMenuItem {
        text: controller.asrRunning ? qsTr("Остановить обработку")
              : controller.translateToRussian ? qsTr("Распознать и перевести") : qsTr("Распознать речь")
        enabled: controller.asrRunning || (controller.hasMedia && controller.seekable
                 && controller.duration > 0 && controller.audioTrack >= 0)
        onTriggered: window.toggleRecognition()
    }

    component SettingsField: TextField {
        id: field
        hoverEnabled: true
        leftPadding: 12
        rightPadding: 12
        background: Rectangle {
            implicitHeight: 34
            radius: 10
            color: field.activeFocus ? "#2f3542" : field.hovered ? "#2a2e38" : "#242832"
            border.color: field.activeFocus ? "#93c9ff" : "#38ffffff"
            Behavior on color { ColorAnimation { duration: 140 } }
            Behavior on border.color { ColorAnimation { duration: 140 } }
        }
    }

    component SettingsComboBox: ComboBox {
        id: combo
        hoverEnabled: true
        background: Rectangle {
            implicitHeight: 34
            radius: 10
            color: combo.hovered ? "#2f3542" : "#242832"
            border.color: combo.visualFocus ? "#93c9ff" : "#38ffffff"
            Behavior on color { ColorAnimation { duration: 140 } }
        }
    }

    component SettingsSwitch: Switch {
        id: toggle
        hoverEnabled: true
        padding: 3
        spacing: 8
        indicator: Rectangle {
            implicitWidth: 34
            implicitHeight: 20
            x: toggle.leftPadding
            y: (toggle.height - height) / 2
            radius: height / 2
            opacity: toggle.enabled ? 1 : 0.55
            color: toggle.checked ? (toggle.hovered ? "#498ee3" : "#337bd3")
                                  : (toggle.hovered ? "#535968" : "#3a404d")
            border.width: toggle.visualFocus ? 1 : 0
            border.color: "#93c9ff"
            Behavior on color { ColorAnimation { duration: 140 } }
            Rectangle {
                width: 14
                height: width
                radius: width / 2
                x: 3 + (parent.width - width - 6) * toggle.visualPosition
                anchors.verticalCenter: parent.verticalCenter
                color: "#eef0f6"
                Behavior on x {
                    enabled: !toggle.down
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }
            }
        }
        contentItem: Text {
            leftPadding: toggle.indicator.width + toggle.spacing
            text: toggle.text
            font: toggle.font
            color: toggle.enabled ? "#eef0f6" : "#9ca2b0"
            verticalAlignment: Text.AlignVCenter
        }
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: videoDialog.open() }
    Shortcut { sequence: "Space"; onActivated: { stage.revealControls(); controller.togglePlayback() } }
    Shortcut { sequence: "Left"; onActivated: { stage.revealControls(); controller.skip(-5000) } }
    Shortcut { sequence: "Right"; onActivated: { stage.revealControls(); controller.skip(5000) } }
    Shortcut { sequence: "F11"; onActivated: { stage.revealControls(); window.toggleFullScreen() } }
    Shortcut {
        sequence: "Escape"
        enabled: window.visibility === Window.FullScreen
        onActivated: { stage.revealControls(); window.showNormal() }
    }

    FileDialog {
        id: videoDialog
        title: qsTr("Открыть видео")
        nameFilters: [qsTr("Видео (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.mpeg *.mpg *.ts)"), qsTr("Все файлы (*)")]
        onAccepted: controller.openMedia(selectedFile)
    }
    FileDialog {
        id: subtitleDialog
        property bool translation: false
        title: translation ? qsTr("Открыть перевод SRT") : qsTr("Открыть оригинальные субтитры SRT")
        nameFilters: [qsTr("Субтитры SubRip (*.srt)")]
        onAccepted: controller.openSubtitles(selectedFile, translation)
    }
    FileDialog {
        id: modelDialog
        title: qsTr("Выбрать модель Whisper")
        nameFilters: [qsTr("Модели Whisper (*.bin)")]
        onAccepted: controller.setWhisperModel(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Сохранить оригинальные субтитры")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Субтитры SubRip (*.srt)")]
        defaultSuffix: "srt"
        onAccepted: controller.saveOriginal(selectedFile)
    }
    Dialog {
        id: asrSettings
        title: qsTr("Настройки")
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, window.width - 40)
        padding: 16
        header: null
        footer: null
        background: Rectangle {
            radius: 20
            color: "#22252e"
            border.color: "#40ffffff"
        }
        contentItem: ColumnLayout {
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                Label {
                    Layout.fillWidth: true
                    text: asrSettings.title
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Label { text: qsTr("Язык интерфейса") }
                SettingsComboBox {
                    Layout.preferredWidth: 132
                    model: ["English", "Русский"]
                    currentIndex: interfaceLanguage.language === "en" ? 0 : 1
                    Accessible.name: qsTr("Язык интерфейса")
                    onActivated: index => interfaceLanguage.language = index === 0 ? "en" : "ru"
                }
            }
            Label { text: qsTr("Модель Whisper (.bin)") }
            RowLayout {
                Layout.fillWidth: true
                SettingsField {
                    Layout.fillWidth: true
                    text: controller.modelPath
                    readOnly: true
                    placeholderText: qsTr("Выберите ggml-base.en.bin")
                    Accessible.name: qsTr("Файл модели Whisper")
                }
                ActionButton { text: qsTr("Выбрать…"); onClicked: modelDialog.open() }
            }
            Label { text: "FFmpeg" }
            SettingsField {
                Layout.fillWidth: true
                text: controller.ffmpegPath
                placeholderText: qsTr("Путь к ffmpeg")
                Accessible.name: qsTr("Путь к FFmpeg")
                onEditingFinished: {
                    if (text !== controller.ffmpegPath) controller.ffmpegPath = text
                }
            }
            SettingsSwitch {
                text: qsTr("Использовать GPU, если доступен")
                checked: controller.useGpu
                onToggled: controller.useGpu = checked
            }
            SettingsSwitch {
                text: qsTr("Переводить на русский во время просмотра")
                checked: controller.translateToRussian
                onToggled: controller.translateToRussian = checked
            }
            SettingsField {
                Layout.fillWidth: true
                visible: controller.translateToRussian
                text: controller.translationModel
                placeholderText: qsTr("Модель Ollama, например qwen2.5:3b")
                Accessible.name: qsTr("Локальная модель перевода Ollama")
                onEditingFinished: controller.translationModel = text
            }
            Label {
                Layout.fillWidth: true
                visible: controller.translateToRussian
                text: qsTr("Для перевода запустите Ollama и загрузите модель командой ollama pull %1.").arg(controller.translationModel)
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: "#b5c1d3"
            }
            Label {
                Layout.fillWidth: true
                visible: controller.asrStatus.length > 0
                text: controller.asrStatus
                wrapMode: Text.Wrap
                color: "#b5c1d3"
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#303a49" }
            RowLayout {
                SettingsSwitch {
                    text: qsTr("Оригинал")
                    checked: window.showOriginal
                    onToggled: window.showOriginal = checked
                }
                SettingsSwitch {
                    text: qsTr("Перевод")
                    checked: window.showTranslation
                    onToggled: window.showTranslation = checked
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Громкость") }
                PlayerSlider {
                    Layout.fillWidth: true
                    from: 0; to: 1
                    value: controller.volume
                    onMoved: controller.volume = value
                    Accessible.name: qsTr("Громкость")
                }
            }
            SettingsComboBox {
                Layout.fillWidth: true
                visible: controller.audioTracks.length > 1
                model: controller.audioTracks
                currentIndex: controller.audioTrack
                onActivated: controller.audioTrack = currentIndex
                Accessible.name: qsTr("Аудиодорожка")
            }
            ActionButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 4
                text: qsTr("Закрыть")
                onClicked: asrSettings.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? errorRow.implicitHeight + 20 : 0
            visible: controller.error.length > 0
            color: "#3c252b"
            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.margins: 10
                Label {
                    Layout.fillWidth: true
                    text: controller.error
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: "#ffcad2"
                }
                ActionButton { text: "✕"; hint: qsTr("Закрыть сообщение"); onClicked: controller.dismissError() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: stage
                objectName: "videoStage"
                property bool controlsShown: true
                readonly property bool controlsHeld: pointerPress.active || progress.pressed
                    || fileMenu.visible || videoMenu.visible || asrSettings.visible || videoDialog.visible
                    || subtitleDialog.visible || modelDialog.visible || exportDialog.visible
                focus: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#080b11"
                clip: true

                function revealControls() {
                    controlsShown = true
                    if (controller.hasMedia && !controlsHeld) controlsTimeout.restart()
                    else controlsTimeout.stop()
                }

                onControlsHeldChanged: revealControls()
                Keys.onPressed: event => {
                    if (!controlsShown && event.key === Qt.Key_Tab) {
                        revealControls()
                        fileButton.forceActiveFocus(Qt.TabFocusReason)
                        event.accepted = true
                    } else {
                        revealControls()
                        event.accepted = false
                    }
                }

                Timer {
                    id: controlsTimeout
                    interval: 3000
                    onTriggered: {
                        for (let item = window.activeFocusItem; item; item = item.parent) {
                            if (item === commandBar || item === playbackBar) {
                                stage.forceActiveFocus()
                                break
                            }
                        }
                        stage.controlsShown = false
                    }
                }
                Connections {
                    target: controller
                    function onSourceChanged() { stage.revealControls() }
                }
                Component.onCompleted: revealControls()
                HoverHandler {
                    property point lastPosition: Qt.point(-1, -1)
                    onPointChanged: {
                        const position = point.scenePosition
                        if (hovered && (position.x !== lastPosition.x || position.y !== lastPosition.y)) {
                            lastPosition = position
                            stage.revealControls()
                        }
                    }
                }
                PointHandler {
                    id: pointerPress
                    target: null
                }

                VideoOutput {
                    id: video
                    objectName: "videoOutput"
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                    Component.onCompleted: controller.setVideoSink(videoSink)
                }
                MouseArea {
                    id: videoPointer
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            videoMenu.popup(videoPointer, mouse.x, mouse.y)
                        else if (controller.hasMedia)
                            controller.togglePlayback()
                        else
                            videoDialog.open()
                    }
                    onDoubleClicked: mouse => {
                        if (mouse.button === Qt.LeftButton && controller.hasMedia)
                            window.toggleFullScreen()
                    }
                }
                PlayerMenu {
                    id: videoMenu
                    OpenVideoMenuItem {}
                    RecognitionMenuItem {
                        visible: controller.asrAvailable
                        height: visible ? implicitHeight : 0
                    }
                    PlayerMenu {
                        id: speedMenu
                        width: 160
                        title: qsTr("Скорость · %1").arg(window.rateLabel(controller.playbackRate))
                        enabled: controller.hasMedia && controller.duration > 0
                        Instantiator {
                            model: [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2]
                            delegate: PlayerMenuItem {
                                required property real modelData
                                text: window.rateLabel(modelData)
                                checkable: true
                                checked: Math.abs(controller.playbackRate - modelData) < 0.001
                                onTriggered: controller.playbackRate = modelData
                            }
                            onObjectAdded: (index, object) => speedMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => speedMenu.removeItem(object)
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: !controller.hasMedia
                    text: qsTr("Нажмите, чтобы открыть видео, или перетащите файл")
                    color: "#a2a9b9"
                    font.pixelSize: 16
                }
                BusyIndicator { anchors.centerIn: parent; running: controller.busy; visible: running }
                Rectangle {
                    id: captions
                    anchors.bottom: playbackBar.top
                    anchors.bottomMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width - 48, subtitleColumn.implicitWidth + 32)
                    height: subtitleColumn.implicitHeight + 20
                    color: "#e512141b"
                    radius: 14
                    visible: (window.showOriginal && controller.originalText.length > 0)
                             || (window.showTranslation && controller.translatedText.length > 0)
                    ColumnLayout {
                        id: subtitleColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7
                        Label {
                            Layout.fillWidth: true
                            Layout.maximumWidth: stage.width - 80
                            visible: window.showOriginal && text.length > 0
                            text: controller.originalText
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: controller.translatedText.length > 0 && window.showTranslation ? 16 : 21
                            color: "#d2d9e5"
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.maximumWidth: stage.width - 80
                            visible: window.showTranslation && text.length > 0
                            text: controller.translatedText
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 21
                            font.weight: Font.DemiBold
                            color: "#ffffff"
                        }
                    }
                }
                DropArea {
                    anchors.fill: parent
                    onDropped: function(drop) {
                        if (!drop.hasUrls || drop.urls.length === 0) return
                        const url = drop.urls[0]
                        if (url.toString().toLowerCase().endsWith(".srt")) controller.openSubtitles(url)
                        else controller.openMedia(url)
                        drop.acceptProposedAction()
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: parent.containsDrag
                        color: "#441b5a50"
                        border.color: "#87ddc8"
                        border.width: 2
                    }
                }

                VideoControls {
                    id: commandBar
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 14
                    width: commands.implicitWidth + 8
                    height: implicitHeight
                    topTint: "#5c10121a"
                    bottomTint: "#3810121a"
                    border.color: "#25ffffff"
                    RowLayout {
                        id: commands
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 2
                        ActionButton {
                            id: fileButton
                            toolbarItem: true
                            text: "•••"
                            font.pixelSize: 16
                            hint: controller.asrRunning && controller.asrStatus.length
                                  ? qsTr("Файл · %1").arg(controller.asrStatus) : qsTr("Файл")
                            onClicked: fileMenu.open()
                            PlayerMenu {
                                id: fileMenu
                                y: fileButton.height + 8
                                OpenVideoMenuItem {}
                                PlayerMenuItem {
                                    text: qsTr("Оригинал SRT…")
                                    enabled: controller.hasMedia
                                    onTriggered: { subtitleDialog.translation = false; subtitleDialog.open() }
                                }
                                PlayerMenuItem {
                                    text: qsTr("Перевод SRT…")
                                    enabled: controller.hasMedia
                                    onTriggered: { subtitleDialog.translation = true; subtitleDialog.open() }
                                }
                                MenuSeparator {}
                                PlayerMenuItem {
                                    text: controller.asrStatus
                                    visible: controller.asrRunning && text.length > 0
                                    height: visible ? implicitHeight : 0
                                    enabled: false
                                }
                                RecognitionMenuItem {
                                    visible: controller.asrAvailable
                                    height: visible ? implicitHeight : 0
                                }
                                PlayerMenuItem {
                                    text: qsTr("Реплики")
                                    enabled: window.showTranscript || controller.transcript.count > 0
                                    checkable: true
                                    checked: window.showTranscript
                                    onTriggered: window.showTranscript = checked
                                }
                                PlayerMenuItem {
                                    text: qsTr("Сохранить оригинал SRT…")
                                    enabled: controller.originalFile.length > 0 && controller.transcript.count > 0
                                    onTriggered: exportDialog.open()
                                }
                                PlayerMenuItem {
                                    text: qsTr("Убрать субтитры")
                                    enabled: controller.originalFile.length > 0 || controller.translatedFile.length > 0
                                    onTriggered: controller.clearSubtitles()
                                }
                            }
                        }
                        ActionButton {
                            toolbarItem: true
                            text: "⚙"
                            font.pixelSize: 18
                            hint: qsTr("Настройки")
                            onClicked: asrSettings.open()
                        }
                        ActionButton {
                            toolbarItem: true
                            text: window.visibility === Window.FullScreen ? "↙" : "⛶"
                            font.pixelSize: 18
                            hint: window.visibility === Window.FullScreen ? qsTr("Выйти из полного экрана · Esc") : qsTr("Полный экран · F11")
                            onClicked: window.toggleFullScreen()
                        }
                    }
                }

                VideoControls {
                    id: playbackBar
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 14
                    height: implicitHeight
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 12
                        spacing: 7
                        ActionButton {
                            objectName: "playButton"
                            toolbarItem: true
                            text: controller.playRequested ? "Ⅱ" : "▶"
                            font.pixelSize: 16
                            enabled: controller.hasMedia
                            hint: controller.playRequested ? qsTr("Пауза · Пробел") : qsTr("Воспроизвести · Пробел")
                            onClicked: controller.togglePlayback()
                        }
                        PlayerSlider {
                            id: progress
                            objectName: "seekSlider"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            enabled: controller.seekable
                            from: 0
                            to: Math.max(1, controller.duration)
                            value: controller.position
                            onMoved: controller.seek(Math.round(value))
                            Accessible.name: qsTr("Позиция воспроизведения")
                            ToolTip.visible: pressed
                            ToolTip.text: controller.timeLabel(value)
                        }
                        Label {
                            text: controller.timeLabel(progress.pressed ? progress.value : controller.position)
                                  + " / " + controller.timeLabel(controller.duration)
                            color: "#dfe3ed"
                            font.pixelSize: 12
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: Math.min(300, window.width * 0.26)
                Layout.fillHeight: true
                visible: window.showTranscript
                color: "#151d29"
                border.color: "#263044"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Реплики"); font.pixelSize: 15; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        Label { text: controller.transcript.count; color: "#87ddc8" }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: controller.originalFile || controller.translatedFile || qsTr("Субтитры ещё не добавлены")
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        color: "#7e8ba2"
                        font.pixelSize: 11
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#263044" }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Label {
                            anchors.centerIn: parent
                            width: parent.width - 12
                            visible: controller.transcript.count === 0
                            text: qsTr("Запустите распознавание\nили добавьте файл SRT.\n\nНажмите на реплику, чтобы перейти к ней в видео.")
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            color: "#7e8ba2"
                            lineHeight: 1.4
                        }
                        ListView {
                            id: transcriptList
                            anchors.fill: parent
                            clip: true
                            spacing: 6
                            model: controller.transcript
                            ScrollBar.vertical: ScrollBar {
                                id: transcriptScrollBar
                                contentItem: Rectangle {
                                    implicitWidth: 4
                                    color: "#768399"
                                    visible: transcriptScrollBar.size < 1
                                }
                            }
                            delegate: ItemDelegate {
                                id: cueDelegate
                                required property string cueText
                                required property string timeLabel
                                required property double startMs
                                required property bool active
                                width: ListView.view.width
                                height: cueContent.implicitHeight + 20
                                hoverEnabled: true
                                Accessible.name: timeLabel + " " + cueText
                                onClicked: controller.seek(startMs)
                                background: Rectangle {
                                    radius: 10
                                    color: cueDelegate.active ? "#243e3f" : cueDelegate.hovered ? "#202a3b" : "transparent"
                                    border.width: cueDelegate.visualFocus ? 1 : 0
                                    border.color: "#93c9ff"
                                    Behavior on color { ColorAnimation { duration: 140 } }
                                }
                                contentItem: ColumnLayout {
                                    id: cueContent
                                    spacing: 5
                                    Label { text: cueDelegate.timeLabel; font.pixelSize: 11; color: cueDelegate.active ? "#87ddc8" : "#718198" }
                                    Label {
                                        Layout.fillWidth: true
                                        text: cueDelegate.cueText
                                        textFormat: Text.PlainText
                                        wrapMode: Text.Wrap
                                        color: cueDelegate.active ? "#f0faf7" : "#bdc8d9"
                                    }
                                }
                            }
                            Connections {
                                target: controller.transcript
                                function onCurrentIndexChanged() {
                                    if (controller.transcript.currentIndex >= 0 && !transcriptList.moving)
                                        transcriptList.positionViewAtIndex(controller.transcript.currentIndex, ListView.Contain)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
