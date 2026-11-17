#include "audioextractor.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QtEndian>
#include <bit>

AudioSamples extractAudio(const QString &ffmpeg, const QString &media,
                          int audioTrack, qint64 startMs, qint64 durationMs,
                          const std::atomic_bool &cancelled)
{
    AudioSamples result;
    if (cancelled.load()) return result;
    if (durationMs <= 0 || durationMs > 30000 || startMs < 0 || audioTrack < 0) {
        result.error = QStringLiteral("Некорректный диапазон аудио.");
        return result;
    }
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    const auto seconds = [](qint64 time) { return QString::number(double(time) / 1000.0, 'f', 3); };
    process.start(ffmpeg, {"-nostdin", "-hide_banner", "-loglevel", "error",
                          "-ss", seconds(startMs), "-i", media,
                          "-map", QStringLiteral("0:a:%1").arg(audioTrack),
                          "-t", seconds(durationMs), "-vn", "-sn", "-dn",
                          "-ac", "1", "-ar", "16000",
                          "-af", "aresample=16000:async=1:first_pts=0",
                          "-f", "f32le", "pipe:1"});
    if (!process.waitForStarted(5000)) {
        result.error = QStringLiteral("Не удалось запустить FFmpeg: %1").arg(process.errorString());
        return result;
    }
    QByteArray output;
    QByteArray diagnostics;
    const qint64 byteLimit = (durationMs + 1000) * 16 * qint64(sizeof(float));
    QElapsedTimer timeout;
    timeout.start();
    while (process.state() != QProcess::NotRunning) {
        if (cancelled.load() || timeout.elapsed() > 60000 || output.size() > byteLimit) {
            process.kill();
            process.waitForFinished(5000);
            if (!cancelled.load())
                result.error = QStringLiteral("FFmpeg превысил время или размер окна обработки.");
            return result;
        }
        process.waitForReadyRead(50);
        output.append(process.readAllStandardOutput());
        diagnostics.append(process.readAllStandardError());
        diagnostics = diagnostics.right(4096);
    }
    output.append(process.readAllStandardOutput());
    diagnostics.append(process.readAllStandardError());
    if (cancelled.load()) return result;
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.error = QStringLiteral("Не удалось извлечь аудио. %1")
                           .arg(QString::fromUtf8(diagnostics.right(4096)).trimmed());
        return result;
    }
    if (output.size() > byteLimit || output.size() % 4 != 0) {
        result.error = QStringLiteral("FFmpeg вернул некорректный аудиобуфер.");
        return result;
    }
    result.pcm.resize(output.size() / 4);
    for (qsizetype i = 0; i < result.pcm.size(); ++i) {
        const quint32 bits = qFromLittleEndian<quint32>(output.constData() + 4 * i);
        result.pcm[i] = std::bit_cast<float>(bits);
    }
    return result;
}
