#pragma once

#include "subtitles.h"
#include <QUrl>
#include <atomic>

struct TranslationResult {
    QVector<SubtitleCue> cues;
    QString error;
};

// Called only from the ASR worker. No audio or text leaves the local machine.
TranslationResult translateCues(const QVector<SubtitleCue> &cues, const QString &model,
                                const std::atomic_bool &cancelled,
                                const QUrl &endpoint = QUrl("http://127.0.0.1:11434/api/generate"),
                                int timeoutMs = 120000);
