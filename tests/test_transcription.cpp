#include "audioextractor.h"
#include "transcription.h"
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class TranscriptionTests : public QObject
{
    Q_OBJECT
    QTemporaryDir m_directory;
    QString m_ffmpeg;
    QString m_audio;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<SubtitleCue>>();
        m_ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        m_audio = m_directory.filePath("sample with spaces.wav");
        QFile file(m_audio);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        const quint32 length = 16000 * 2 * 3;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + length);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(16000)
               << quint32(32000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4);
        stream << length;
        // Nonzero deterministic audio, used only to verify PCM boundaries.
        for (int i = 0; i < 16000 * 3; ++i) stream << qint16(i < 16000 ? 0 : 8192);
    }

    void extractsIndependentWindow()
    {
        if (m_ffmpeg.isEmpty()) QSKIP("FFmpeg is not installed");
        std::atomic_bool cancel{false};
        const auto audio = extractAudio(m_ffmpeg, m_audio, 0, 1000, 1000, cancel);
        QVERIFY2(audio.error.isEmpty(), qPrintable(audio.error));
        QCOMPARE(audio.pcm.size(), 16000);
        QVERIFY(std::abs(audio.pcm[8000] - 0.25f) < 0.001f);
        const auto silence = extractAudio(m_ffmpeg, m_audio, 0, 0, 500, cancel);
        QCOMPARE(silence.pcm.size(), 8000);
        QCOMPARE(silence.pcm[2000], 0.0f);
    }

    void handlesInvalidAndCancelledRequests()
    {
        std::atomic_bool cancel{false};
        QVERIFY(!extractAudio("/not/a/program", m_audio, 0, 0, 1000, cancel).error.isEmpty());
        QVERIFY(!extractAudio(m_ffmpeg, m_audio, 0, 0, 31000, cancel).error.isEmpty());
        cancel.store(true);
        const auto audio = extractAudio(m_ffmpeg, m_audio, 0, 0, 1000, cancel);
        QVERIFY(audio.pcm.isEmpty());
        QVERIFY(audio.error.isEmpty());
    }

    void rejectsStaleResultsAfterCancellation()
    {
        TranscriptionService service;
        QSignalSpy chunks(&service, &TranscriptionService::chunkReady);
        QSignalSpy errors(&service, &TranscriptionService::failed);
        service.start({m_audio, "/missing/model.bin", m_ffmpeg, 0, 0, 3000, false});
        service.cancel();
        service.shutdown();
        QCoreApplication::processEvents();
        QCOMPARE(chunks.count(), 0);
        QCOMPARE(errors.count(), 0);
        QVERIFY(!service.running());
    }

    void reportsModelFailure()
    {
        if (!TranscriptionService::available()) QSKIP("Built without Whisper");
        TranscriptionService service;
        QSignalSpy errors(&service, &TranscriptionService::failed);
        service.start({m_audio, "/missing/model.bin", m_ffmpeg, 0, 0, 3000, false});
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 5000);
        QTRY_VERIFY(!service.running());
    }

    void realModelSmokeTest()
    {
        const auto model = qEnvironmentVariable("QREPLI_TEST_MODEL");
        const auto media = qEnvironmentVariable("QREPLI_TEST_AUDIO");
        if (model.isEmpty() || media.isEmpty() || !TranscriptionService::available())
            QSKIP("Set QREPLI_TEST_MODEL and QREPLI_TEST_AUDIO to test real inference");
        QVERIFY(!m_ffmpeg.isEmpty());
        TranscriptionService service;
        QSignalSpy errors(&service, &TranscriptionService::failed);
        QSignalSpy chunks(&service, &TranscriptionService::chunkReady);
        QElapsedTimer timer;
        timer.start();
        service.start({media, model, m_ffmpeg, 0, 0, 20000, true});
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 120000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first().first().toString()));
        QCOMPARE(chunks.count(), 1);
        const auto cues = qvariant_cast<QVector<SubtitleCue>>(chunks.first()[0]);
        QVERIFY(!cues.isEmpty());
        QString text;
        for (const auto &cue : cues) {
            QVERIFY(cue.endMs > cue.startMs);
            QVERIFY(cue.startMs >= 0 && cue.endMs <= 20000);
            text += cue.text + ' ';
        }
        qInfo().noquote() << "ASR elapsed ms:" << timer.elapsed() << "Transcript:" << text;
        QVERIFY2(text.contains("video", Qt::CaseInsensitive), qPrintable(text));
    }

    void boundedLookaheadAndRestart()
    {
        const auto model = qEnvironmentVariable("QREPLI_TEST_MODEL");
        const auto media = qEnvironmentVariable("QREPLI_TEST_LONG_AUDIO");
        if (model.isEmpty() || media.isEmpty() || !TranscriptionService::available())
            QSKIP("Set QREPLI_TEST_MODEL and QREPLI_TEST_LONG_AUDIO for the long scenario");
        TranscriptionService service;
        QSignalSpy errors(&service, &TranscriptionService::failed);
        QSignalSpy chunks(&service, &TranscriptionService::chunkReady);
        service.start({media, model, m_ffmpeg, 0, 0, 65000, true});
        QTRY_VERIFY_WITH_TIMEOUT(chunks.size() >= 2 || !errors.isEmpty(), 120000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(chunks.size(), 2);
        QCOMPARE(chunks.last()[1].toLongLong(), 40000);
        QVERIFY(service.running());
        // With playhead at zero the decoder must wait instead of reading the
        // remainder. Observe its explicit wait state, then replace the job.
        QTRY_VERIFY(service.status().contains("00:40"));
        QCOMPARE(chunks.size(), 2);
        chunks.clear();
        service.start({media, model, m_ffmpeg, 0, 55000, 65000, true});
        QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 120000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(chunks.size(), 1);
        QCOMPARE(chunks.first()[1].toLongLong(), 65000);
        for (const auto &cue : qvariant_cast<QVector<SubtitleCue>>(chunks.first()[0]))
            QVERIFY(cue.startMs >= 55000);
    }
};

QTEST_GUILESS_MAIN(TranscriptionTests)
#include "test_transcription.moc"
