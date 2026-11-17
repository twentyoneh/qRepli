import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import qRepli

ApplicationWindow {
    id: window
    required property PlayerController controller
    property bool showOriginal: true
    property bool showTranslation: true
    property bool showTranscript: true
    width: 1180
    height: 760
    minimumWidth: 820
    minimumHeight: 540
    visible: true
    title: controller.hasMedia ? controller.fileName + " — qRepli" : "qRepli"
    color: "#101620"
    font.family: Qt.platform.os === "osx" ? "Helvetica Neue" : "Segoe UI"
    font.pixelSize: 13
    palette.window: "#101620"
    palette.windowText: "#e4e9f2"
    palette.text: "#e4e9f2"
    palette.buttonText: "#e4e9f2"
    palette.base: "#192231"
    palette.button: "#202a3a"
    palette.highlight: "#87ddc8"
    palette.highlightedText: "#101620"
    palette.mid: "#3b475d"
    palette.dark: "#0b0f16"

    function toggleFullScreen() {
        if (visibility === Window.FullScreen) showNormal()
        else showFullScreen()
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: videoDialog.open() }
    Shortcut { sequence: "Space"; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Left"; onActivated: controller.skip(-5000) }
    Shortcut { sequence: "Right"; onActivated: controller.skip(5000) }
    Shortcut { sequence: "F11"; onActivated: window.toggleFullScreen() }
    Shortcut { sequence: "Escape"; enabled: window.visibility === Window.FullScreen; onActivated: window.showNormal() }

    FileDialog {
        id: videoDialog
        title: "Открыть видео"
        nameFilters: ["Видео (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.mpeg *.mpg *.ts)", "Все файлы (*)"]
        onAccepted: controller.openMedia(selectedFile)
    }
    FileDialog {
        id: subtitleDialog
        property bool translation: false
        title: translation ? "Открыть перевод SRT" : "Открыть оригинальные субтитры SRT"
        nameFilters: ["Субтитры SubRip (*.srt)"]
        onAccepted: controller.openSubtitles(selectedFile, translation)
    }
    FileDialog {
        id: modelDialog
        title: "Выбрать модель Whisper"
        nameFilters: ["Модели Whisper (*.bin)"]
        onAccepted: controller.setWhisperModel(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: "Сохранить оригинальные субтитры"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Субтитры SubRip (*.srt)"]
        defaultSuffix: "srt"
        onAccepted: controller.saveOriginal(selectedFile)
    }
    Dialog {
        id: asrSettings
        title: "Распознавание речи"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, window.width - 40)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: "Английская речь распознаётся на этом компьютере. Для начала выберите модель Whisper."
                wrapMode: Text.Wrap
                color: "#b5c1d3"
            }
            Label { text: "Модель Whisper (.bin)" }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    Layout.fillWidth: true
                    text: controller.modelPath
                    readOnly: true
                    placeholderText: "Выберите ggml-base.en.bin"
                    Accessible.name: "Файл модели Whisper"
                }
                ActionButton { text: "Выбрать…"; onClicked: modelDialog.open() }
            }
            Label { text: "FFmpeg" }
            TextField {
                Layout.fillWidth: true
                text: controller.ffmpegPath
                placeholderText: "Путь к ffmpeg"
                Accessible.name: "Путь к FFmpeg"
                onEditingFinished: {
                    if (text !== controller.ffmpegPath) controller.ffmpegPath = text
                }
            }
            CheckBox {
                text: "Использовать GPU, если доступен"
                checked: controller.useGpu
                onToggled: controller.useGpu = checked
            }
            Label {
                Layout.fillWidth: true
                text: "Первый фрагмент готовится перед просмотром. Перемотка за пределы подготовленного участка запускает распознавание с новой позиции."
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: "#8e9bb0"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "#151d29"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 10
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: "#87ddc8"
                    Text { anchors.centerIn: parent; text: "▶"; color: "#11282a"; font.pixelSize: 15 }
                }
                Label { text: "qRepli"; font.pixelSize: 19; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                ActionButton {
                    text: "Оригинал SRT"
                    enabled: controller.hasMedia
                    hint: controller.originalFile || "Загрузить английские субтитры"
                    onClicked: { subtitleDialog.translation = false; subtitleDialog.open() }
                }
                ActionButton {
                    text: "Перевод SRT"
                    enabled: controller.hasMedia
                    hint: controller.translatedFile || "Загрузить русские субтитры"
                    onClicked: { subtitleDialog.translation = true; subtitleDialog.open() }
                }
                ActionButton {
                    text: "Открыть видео"
                    accent: true
                    hint: "Открыть локальный видеофайл"
                    onClicked: videoDialog.open()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 54 : 0
            visible: controller.asrAvailable
            color: "#111e26"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 12
                BusyIndicator { running: controller.asrRunning; visible: running; Layout.preferredWidth: 26; Layout.preferredHeight: 26 }
                Label {
                    Layout.fillWidth: true
                    text: controller.asrStatus || "Английская речь → субтитры"
                    color: "#b3c6ce"
                    elide: Text.ElideRight
                }
                ActionButton { text: "Настройки речи"; onClicked: asrSettings.open() }
                ActionButton {
                    text: controller.asrRunning ? "Остановить" : "Распознать речь"
                    accent: !controller.asrRunning
                    enabled: controller.hasMedia
                    onClicked: {
                        if (controller.asrRunning) controller.stopTranscription()
                        else if (!controller.modelPath.length || !controller.ffmpegPath.length) asrSettings.open()
                        else controller.startTranscription()
                    }
                }
            }
        }

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
                ActionButton { text: "✕"; hint: "Закрыть сообщение"; onClicked: controller.dismissError() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: controller.hasMedia ? controller.fileName : "Ваша история начинается с видео"
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                        color: "#aeb9cc"
                    }
                    Label {
                        text: controller.status
                        color: controller.playing ? "#87ddc8" : "#7e8ba2"
                        font.pixelSize: 11
                    }
                }
                Rectangle {
                    id: stage
                    objectName: "videoStage"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#080b11"
                    radius: 12
                    clip: true
                    border.color: "#263044"
                    VideoOutput {
                        id: video
                        objectName: "videoOutput"
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: VideoOutput.PreserveAspectFit
                        Component.onCompleted: controller.setVideoSink(videoSink)
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: controller.hasMedia
                        onClicked: controller.togglePlayback()
                        onDoubleClicked: window.toggleFullScreen()
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 60, 380)
                        spacing: 18
                        visible: !controller.hasMedia
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 64; height: 64; radius: 20
                            color: "#182c30"
                            Text { anchors.centerIn: parent; text: "▶"; font.pixelSize: 27; color: "#87ddc8" }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Видео. Два языка."
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Откройте видео и распознайте речь\nили добавьте свои субтитры."
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            color: "#8e9bb0"
                            lineHeight: 1.4
                        }
                        ActionButton {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Выбрать видео"
                            accent: true
                            onClicked: videoDialog.open()
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "или перетащите файл в окно"
                            font.pixelSize: 11
                            color: "#626f85"
                        }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: controller.busy; visible: running }
                    Rectangle {
                        id: captions
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width - 48, subtitleColumn.implicitWidth + 32)
                        height: subtitleColumn.implicitHeight + 20
                        color: "#dd0c1018"
                        radius: 8
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
                            radius: 12
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        text: "Оригинал"
                        checked: window.showOriginal
                        enabled: controller.originalFile.length > 0
                        onToggled: window.showOriginal = checked
                    }
                    CheckBox {
                        text: "Перевод"
                        checked: window.showTranslation
                        enabled: controller.translatedFile.length > 0
                        onToggled: window.showTranslation = checked
                    }
                    Item { Layout.fillWidth: true }
                    ActionButton {
                        text: "Реплики"
                        checkable: true
                        checked: window.showTranscript
                        onClicked: window.showTranscript = checked
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: Math.min(300, window.width * 0.26)
                Layout.fillHeight: true
                visible: window.showTranscript
                radius: 12
                color: "#151d29"
                border.color: "#263044"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Реплики"; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        Label { text: controller.transcript.count; color: "#87ddc8" }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: controller.originalFile || controller.translatedFile || "Субтитры ещё не добавлены"
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
                            text: "Запустите распознавание\nили добавьте файл SRT.\n\nНажмите на реплику, чтобы перейти к ней в видео."
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
                            ScrollBar.vertical: ScrollBar {}
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
                                    radius: 8
                                    color: cueDelegate.active ? "#243e3f" : cueDelegate.hovered ? "#202a3b" : "transparent"
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
                    ActionButton {
                        Layout.fillWidth: true
                        visible: controller.transcript.count > 0 && controller.originalFile.length > 0
                        text: "Сохранить SRT"
                        onClicked: exportDialog.open()
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        visible: controller.transcript.count > 0
                        text: "Убрать субтитры"
                        onClicked: controller.clearSubtitles()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            color: "#151d29"
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                anchors.topMargin: 7
                anchors.bottomMargin: 16
                spacing: 6
                Slider {
                    id: progress
                    objectName: "seekSlider"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    enabled: controller.seekable
                    from: 0
                    to: Math.max(1, controller.duration)
                    value: controller.position
                    onMoved: controller.seek(Math.round(value))
                    Accessible.name: "Позиция воспроизведения"
                    ToolTip.visible: pressed
                    ToolTip.text: controller.timeLabel(value)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ActionButton {
                        objectName: "playButton"
                        text: controller.playRequested ? "Ⅱ" : "▶"
                        accent: true
                        implicitWidth: 46
                        enabled: controller.hasMedia
                        hint: controller.playRequested ? "Пауза · Пробел" : "Воспроизвести · Пробел"
                        onClicked: controller.togglePlayback()
                    }
                    ActionButton { text: "−5 с"; enabled: controller.seekable; hint: "Назад на 5 секунд · ←"; onClicked: controller.skip(-5000) }
                    ActionButton { text: "+5 с"; enabled: controller.seekable; hint: "Вперёд на 5 секунд · →"; onClicked: controller.skip(5000) }
                    Label {
                        text: controller.timeLabel(progress.pressed ? progress.value : controller.position)
                              + "  /  " + controller.timeLabel(controller.duration)
                        color: "#aeb9cc"
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        Layout.maximumWidth: 160
                        visible: controller.audioTracks.length > 1
                        model: controller.audioTracks
                        currentIndex: controller.audioTrack
                        onActivated: controller.audioTrack = currentIndex
                        Accessible.name: "Аудиодорожка"
                    }
                    Label { text: "Звук"; color: "#7e8ba2"; font.pixelSize: 11 }
                    Slider {
                        Layout.preferredWidth: 90
                        from: 0; to: 1
                        value: controller.volume
                        onMoved: controller.volume = value
                        Accessible.name: "Громкость"
                    }
                    ActionButton { text: "⛶"; hint: "Полный экран · F11"; onClicked: window.toggleFullScreen() }
                }
            }
        }
    }
}
