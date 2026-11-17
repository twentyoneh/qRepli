#include "playercontroller.h"
#include <QDataStream>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class PlayerTests : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_directory;
    QUrl m_video;
    QUrl writeFile(const QString &name, const QByteArray &contents)
    {
        QFile file(m_directory.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
            return {};
        return QUrl::fromLocalFile(file.fileName());
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("qRepliTests");
        QCoreApplication::setApplicationName("test_player");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_directory.path());
        QByteArray wav;
        QDataStream stream(&wav, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        const quint32 dataSize = 16000 * 2 * 10;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + dataSize);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(16000)
               << quint32(32000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4);
        stream << dataSize;
        wav.append(QByteArray(dataSize, '\0'));
        m_video = writeFile("test.wav", wav);
        QVERIFY(!m_video.isEmpty());
    }

    void playbackSeekAndSubtitleLifecycle()
    {
        PlayerController player;
        player.setVolume(0);
        QVERIFY(player.openMedia(m_video));
        QTRY_VERIFY_WITH_TIMEOUT(player.duration() > 0 && player.seekable(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(player.playing(), 5000);
        player.togglePlayback();
        QTRY_VERIFY(!player.playing());
        QCOMPARE(player.duration(), 10000);

        const auto original = writeFile("en.srt", "1\n00:00:01,000 --> 00:00:03,000\nHello\n");
        const auto translation = writeFile("ru.srt", QString("1\n00:00:01,500 --> 00:00:04,000\nПривет\n").toUtf8());
        QVERIFY(player.openSubtitles(original));
        QVERIFY(player.openSubtitles(translation, true));
        player.seek(2000);
        QTRY_COMPARE(player.position(), 2000);
        QCOMPARE(player.originalText(), QString("Hello"));
        QCOMPARE(player.translatedText(), QString("Привет"));
        QCOMPARE(player.transcript()->currentIndex(), 0);

        player.seek(3500);
        QTRY_COMPARE(player.position(), 3500);
        QCOMPARE(player.originalText(), QString());
        QCOMPARE(player.translatedText(), QString("Привет"));
        player.seek(500);
        QTRY_COMPARE(player.position(), 500);
        QVERIFY(player.originalText().isEmpty());
        QVERIFY(player.translatedText().isEmpty());

        const auto invalid = writeFile("bad.srt", "this is not SRT");
        QVERIFY(!player.openSubtitles(invalid));
        QVERIFY(!player.error().isEmpty());
        QCOMPARE(player.originalFile(), QString("en.srt"));
        QCOMPARE(player.transcript()->count(), 1);

        // Invalid input preserves the current movie and its subtitles.
        QVERIFY(!player.openMedia(QUrl::fromLocalFile(m_directory.filePath("missing.mp4"))));
        QCOMPARE(player.originalFile(), QString("en.srt"));

        // A new source cannot display subtitles from the previous one.
        QVERIFY(player.openMedia(m_video));
        QVERIFY(player.originalFile().isEmpty());
        QVERIFY(player.translatedFile().isEmpty());
        QVERIFY(player.originalText().isEmpty());
        QVERIFY(player.translatedText().isEmpty());
        QCOMPARE(player.transcript()->count(), 0);
    }

    void translationOnlyAndClear()
    {
        PlayerController player;
        player.setVolume(0);
        QVERIFY(!player.openSubtitles(QUrl::fromLocalFile("unused.srt")));
        QVERIFY(player.openMedia(m_video));
        const auto sub = writeFile("translation.srt", "1\n00:00:01,000 --> 00:00:02,000\nTranslation");
        QVERIFY(player.openSubtitles(sub, true));
        QCOMPARE(player.transcript()->count(), 1);
        player.clearSubtitles();
        QCOMPARE(player.transcript()->count(), 0);
        QVERIFY(player.translatedFile().isEmpty());
    }

    void realTranslatedPlayback()
    {
        const QString model = qEnvironmentVariable("QREPLI_TEST_MODEL");
        const QString media = qEnvironmentVariable("QREPLI_TEST_AUDIO");
        if (!qEnvironmentVariableIsSet("QREPLI_TEST_TRANSLATION") || model.isEmpty() || media.isEmpty())
            QSKIP("Set model, audio and QREPLI_TEST_TRANSLATION for full EN-RU integration");
        PlayerController player;
        player.setVolume(0);
        player.setTranslateToRussian(true);
        player.setTranslationModel("qwen2.5:3b");
        player.setWhisperModel(QUrl::fromLocalFile(model));
        QVERIFY(player.openMedia(QUrl::fromLocalFile(media)));
        QTRY_VERIFY(player.seekable() && player.duration() > 0);
        player.startTranscription();
        QVERIFY(!player.playing());
        player.togglePlayback();
        QVERIFY(!player.playRequested());
        QTRY_VERIFY_WITH_TIMEOUT(!player.asrRunning(), 120000);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
        QVERIFY(!player.playing());
        QCOMPARE(player.translatedFile(), QString("Автоперевод · RU"));
        player.seek(2000);
        QVERIFY(!player.originalText().isEmpty());
        QVERIFY(player.translatedText().contains(QRegularExpression("[А-Яа-яЁё]")));
        qInfo().noquote() << "RU subtitles:" << player.translatedText();
        player.togglePlayback();
        QTRY_VERIFY(player.playing());
        // Start again, then change source before results can arrive.
        player.startTranscription();
        QVERIFY(player.openMedia(m_video));
        QCoreApplication::processEvents();
        QVERIFY(player.translatedText().isEmpty());
        QVERIFY(player.translatedFile().isEmpty());
    }

    void realTranscriptionAndSourceChange()
    {
        const QString model = qEnvironmentVariable("QREPLI_TEST_MODEL");
        const QString media = qEnvironmentVariable("QREPLI_TEST_AUDIO");
        if (model.isEmpty() || media.isEmpty() || !TranscriptionService::available())
            QSKIP("Set QREPLI_TEST_MODEL and QREPLI_TEST_AUDIO for ASR integration");
        PlayerController player;
        player.setVolume(0);
        player.setTranslateToRussian(false);
        player.setWhisperModel(QUrl::fromLocalFile(model));
        QVERIFY(player.openMedia(QUrl::fromLocalFile(media)));
        QTRY_VERIFY(player.seekable() && player.duration() > 0);
        player.startTranscription();
        QVERIFY(player.asrRunning());
        QVERIFY(!player.playing());
        QVERIFY(player.playRequested());
        // Pausing during model preparation must not resume playback later.
        player.togglePlayback();
        QVERIFY(!player.playRequested());
        QTRY_VERIFY_WITH_TIMEOUT(!player.asrRunning(), 120000);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
        QVERIFY(player.transcript()->count() > 0);
        QVERIFY(!player.playing());
        player.seek(5000);
        QVERIFY(!player.asrRunning());
        QVERIFY(!player.originalText().isEmpty());
        const auto output = QUrl::fromLocalFile(m_directory.filePath("recognized.srt"));
        QVERIFY(player.saveOriginal(output));
        QString error;
        QVERIFY(!SubtitleTrack::fromFile(output.toLocalFile(), &error).isEmpty());
        QVERIFY(!player.saveOriginal(QUrl::fromLocalFile(media)));

        player.startTranscription();
        QVERIFY(player.asrRunning());
        QVERIFY(player.openMedia(m_video));
        QVERIFY(!player.asrRunning());
        QCOMPARE(player.transcript()->count(), 0);
        QVERIFY(player.originalFile().isEmpty());
        // Any queued result from the previous source must be ignored.
        QCoreApplication::processEvents();
        QCOMPARE(player.transcript()->count(), 0);
    }
};

QTEST_MAIN(PlayerTests)
#include "test_player.moc"
