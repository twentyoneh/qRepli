#include "playercontroller.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("qRepli");
    QCoreApplication::setOrganizationName("qRepli");
    QCoreApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");

    QCommandLineParser arguments;
    arguments.setApplicationDescription(QStringLiteral("Локальный видеоплеер с двумя дорожками SRT."));
    arguments.addHelpOption();  //help
    arguments.addVersionOption();   //version
    arguments.addPositionalArgument("video", QStringLiteral("Локальный видеофайл."), "[video]");
    arguments.addOption({"subtitles", QStringLiteral("Оригинальные субтитры SRT."), "file"});
    arguments.addOption({"translation", QStringLiteral("Переведённые субтитры SRT."), "file"});
    arguments.addOption({"model", QStringLiteral("Локальный файл модели Whisper."), "file"});
    arguments.addOption({"ffmpeg", QStringLiteral("Путь к исполняемому файлу FFmpeg."), "file"});
    arguments.addOption({"transcribe", QStringLiteral("Распознать английскую речь после открытия видео.")});
    arguments.process(app);

    qmlRegisterUncreatableType<PlayerController>("qRepli", 1, 0, "PlayerController",
                                                 "Created by the application");
    qmlRegisterUncreatableType<SubtitleModel>("qRepli", 1, 0, "SubtitleModel",
                                             "Owned by PlayerController");
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
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}});
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
