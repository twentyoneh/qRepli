#include "playercontroller.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

class InterfaceLanguage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    InterfaceLanguage()
    {
        if (QSettings().value("ui/language", "ru").toString() == "en")
            setLanguage(QStringLiteral("en"));
    }

    QString language() const { return m_language; }

    void setLanguage(const QString &language)
    {
        if (language == m_language || (language != "en" && language != "ru")) return;
        if (language == "en") {
            if (!m_english.load(QStringLiteral(":/i18n/qrepli_en.qm"))) {
                qWarning("Could not load English interface translations");
                return;
            }
            QCoreApplication::installTranslator(&m_english);
        } else {
            QCoreApplication::removeTranslator(&m_english);
        }
        m_language = language;
        QSettings().setValue("ui/language", m_language);
        emit languageChanged();
    }

signals:
    void languageChanged();

private:
    QString m_language = QStringLiteral("ru");
    QTranslator m_english;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("qRepli");
    QCoreApplication::setOrganizationName("qRepli");
    QCoreApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    InterfaceLanguage interfaceLanguage;

    QCommandLineParser arguments;
    arguments.setApplicationDescription(QCoreApplication::translate("CommandLine", "Локальный видеоплеер с двумя дорожками SRT."));
    arguments.addHelpOption();  //help
    arguments.addVersionOption();   //version
    arguments.addPositionalArgument("video", QCoreApplication::translate("CommandLine", "Локальный видеофайл."), "[video]");
    arguments.addOption({"subtitles", QCoreApplication::translate("CommandLine", "Оригинальные субтитры SRT."), "file"});
    arguments.addOption({"translation", QCoreApplication::translate("CommandLine", "Переведённые субтитры SRT."), "file"});
    arguments.addOption({"model", QCoreApplication::translate("CommandLine", "Локальный файл модели Whisper."), "file"});
    arguments.addOption({"ffmpeg", QCoreApplication::translate("CommandLine", "Путь к исполняемому файлу FFmpeg."), "file"});
    arguments.addOption({"transcribe", QCoreApplication::translate("CommandLine", "Распознать английскую речь после открытия видео.")});
    arguments.process(app);

    qmlRegisterUncreatableType<PlayerController>("qRepli", 1, 0, "PlayerController",
                                                 "Created by the application");
    qmlRegisterUncreatableType<SubtitleModel>("qRepli", 1, 0, "SubtitleModel",
                                             "Owned by PlayerController");
    qmlRegisterUncreatableType<InterfaceLanguage>("qRepli", 1, 0, "InterfaceLanguage",
                                                 "Created by the application");
    PlayerController controller;
    if (arguments.isSet("model"))
        controller.setWhisperModel(QUrl::fromLocalFile(QFileInfo(arguments.value("model")).absoluteFilePath()));
    if (arguments.isSet("ffmpeg")) controller.setFfmpegPath(arguments.value("ffmpeg"));
    bool requestedTranscription = arguments.isSet("transcribe");
    QObject::connect(&controller, &PlayerController::durationChanged, &controller, [&] {
        if (requestedTranscription && controller.duration() > 0) {  //если распознование было запущено и есть длительность видео
            requestedTranscription = false;
            QTimer::singleShot(0, &controller, &PlayerController::startTranscription);
        }
    });
    QQmlApplicationEngine engine;
    QObject::connect(&interfaceLanguage, &InterfaceLanguage::languageChanged, &engine, [&] {
        engine.retranslate();
        emit controller.stateChanged();
        emit controller.asrChanged();
        emit controller.subtitleFilesChanged();
        emit controller.audioTracksChanged();
    });
    engine.setInitialProperties({
        {"controller", QVariant::fromValue(&controller)},
        {"interfaceLanguage", QVariant::fromValue(&interfaceLanguage)},
        {"openPickerOnStartup", arguments.positionalArguments().isEmpty()}
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("qRepli", "Main");

    if (!arguments.positionalArguments().isEmpty()) {
        QTimer::singleShot(0, &controller, [&] {
            const auto localUrl = [](const QString &path) {
                return QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath());
            };
            if (controller.openMedia(localUrl(arguments.positionalArguments().first()))) {
                if (arguments.isSet("subtitles"))
                    controller.openSubtitles(localUrl(arguments.value("subtitles")));
                if (arguments.isSet("translation"))
                    controller.openSubtitles(localUrl(arguments.value("translation")), true);
            }
        });
    }
    return app.exec();
}

#include "main.moc"
