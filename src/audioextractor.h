#pragma once

#include <QString>
#include <QVector>
#include <atomic>

struct AudioSamples
{
    QVector<float> pcm;
    QString error;
};

// Runs on the ASR worker, never on the GUI thread. A single bounded PCM window
// is decoded independently of QMediaPlayer. No shell or temporary WAV file.
AudioSamples extractAudio(const QString &ffmpeg, const QString &media,
                          int audioTrack, qint64 startMs, qint64 durationMs,
                          const std::atomic_bool &cancelled);
