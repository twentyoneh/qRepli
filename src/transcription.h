#pragma once

#include "subtitles.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include <optional>

struct TranscriptionRequest
{
    QString mediaPath;
    QString modelPath;
    QString ffmpegPath;
    int audioTrack = 0;
    qint64 startMs = 0;
    qint64 durationMs = 0;
    bool useGpu = true;
    bool translateToRussian = false;
    QString translationModel = QStringLiteral("qwen2.5:3b");
};

class TranscriptionService : public QObject
{
    Q_OBJECT
public:
    explicit TranscriptionService(QObject *parent = nullptr) : QObject(parent) {}
    ~TranscriptionService() override;
    static bool available();
    bool running() const { return m_running; }
    QString status() const { return m_status; }
    void start(const TranscriptionRequest &request);
    void cancel();
    void setPosition(qint64 position);
    void shutdown();

signals:
    void stateChanged();
    void translatedChunkReady(const QVector<SubtitleCue> &cues);
    void chunkReady(const QVector<SubtitleCue> &cues, qint64 processedUntilMs);
    void failed(const QString &message);

private:
    struct Control {
        std::atomic_bool cancelled{false};
        std::atomic<qint64> position{0};
    };
    void launch();
    void run(const TranscriptionRequest &request, const std::shared_ptr<Control> &control,
             quint64 generation);
    void postStatus(quint64 generation, const QString &status);
    void postError(quint64 generation, const QString &error);

    QThread *m_thread = nullptr;
    std::shared_ptr<Control> m_control;
    std::optional<TranscriptionRequest> m_pending;
    quint64 m_generation = 0;
    qint64 m_position = 0;
    bool m_running = false;
    QString m_status;
};
