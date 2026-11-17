#include "transcription.h"
#include "audioextractor.h"
#include "translation.h"

#include <QFile>
#include <QElapsedTimer>
#include <QDebug>
#include <algorithm>
#include <cmath>

#ifdef QREPLI_HAS_WHISPER
#include <whisper.h>
#endif

bool TranscriptionService::available()
{
#ifdef QREPLI_HAS_WHISPER
    return true;
#else
    return false;
#endif
}

TranscriptionService::~TranscriptionService() { shutdown(); }

void TranscriptionService::shutdown()
{
    cancel();
    if (m_thread) {
        disconnect(m_thread, nullptr, this, nullptr);
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
}

void TranscriptionService::start(const TranscriptionRequest &request)
{
    cancel();
    m_position = request.startMs;
    m_pending = request;
    m_running = true;
    m_status = QStringLiteral("Подготовка распознавания…");
    emit stateChanged();
    if (!m_thread) launch();
}

void TranscriptionService::cancel()
{
    ++m_generation;
    if (m_control) m_control->cancelled.store(true);
    m_pending.reset();
    if (m_running) {
        m_running = false;
        m_status = QStringLiteral("Распознавание остановлено");
        emit stateChanged();
    }
}

void TranscriptionService::setPosition(qint64 position)
{
    m_position = position;
    if (m_control) m_control->position.store(position);
}

void TranscriptionService::launch()
{
    const auto request = *m_pending;
    m_pending.reset();
    const auto generation = m_generation;
    m_control = std::make_shared<Control>();
    m_control->position.store(m_position);
    const auto control = m_control;
    m_thread = QThread::create([this, request, control, generation] {
        try {
            run(request, control, generation);
        } catch (const std::exception &error) {
            if (!control->cancelled.load())
                postError(generation, QStringLiteral("Ошибка распознавания: %1").arg(QString::fromUtf8(error.what())));
        }
    });
    connect(m_thread, &QThread::finished, this, [this, generation] {
        m_thread->deleteLater();
        m_thread = nullptr;
        if (m_pending) {
            launch();
        } else if (generation == m_generation) {
            m_running = false;
            emit stateChanged();
        }
    });
    m_thread->start();
}

void TranscriptionService::postStatus(quint64 generation, const QString &status)
{
    QMetaObject::invokeMethod(this, [this, generation, status] {
        if (generation != m_generation) return;
        m_status = status;
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void TranscriptionService::postError(quint64 generation, const QString &error)
{
    QMetaObject::invokeMethod(this, [this, generation, error] {
        if (generation != m_generation) return;
        m_status = QStringLiteral("Не удалось распознать речь");
        emit failed(error);
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void TranscriptionService::run(const TranscriptionRequest &request,
                               const std::shared_ptr<Control> &control, quint64 generation)
{
#ifndef QREPLI_HAS_WHISPER
    Q_UNUSED(request)
    Q_UNUSED(control)
    postError(generation, QStringLiteral("Эта сборка создана без Whisper."));
#else
    if (control->cancelled.load()) return;
    postStatus(generation, QStringLiteral("Загрузка модели Whisper…"));
    QElapsedTimer modelTimer;
    modelTimer.start();
    auto contextParams = whisper_context_default_params();
    contextParams.use_gpu = request.useGpu;
    const QByteArray modelPath = QFile::encodeName(request.modelPath);
    auto context = std::unique_ptr<whisper_context, decltype(&whisper_free)>(
        whisper_init_from_file_with_params(modelPath.constData(), contextParams), whisper_free);
    if (!context && request.useGpu && !control->cancelled.load()) {
        postStatus(generation, QStringLiteral("Переключение Whisper на CPU…"));
        contextParams.use_gpu = false;
        context.reset(whisper_init_from_file_with_params(modelPath.constData(), contextParams));
    }
    if (control->cancelled.load()) return;
    if (!context) {
        postError(generation, QStringLiteral("Не удалось загрузить модель Whisper. Выберите корректный файл ggml-*.bin."));
        return;
    }
    qInfo() << "Whisper model loaded in" << modelTimer.elapsed() << "ms";
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = std::clamp(QThread::idealThreadCount() / 2, 1, 4);
    params.language = "en";
    params.translate = false;
    params.no_context = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.suppress_blank = true;
    params.suppress_nst = true;
    params.temperature_inc = 0;
    params.abort_callback = [](void *data) {
        return static_cast<Control *>(data)->cancelled.load();
    };
    params.abort_callback_user_data = control.get();

    constexpr qint64 chunkMs = 20000;
    constexpr qint64 aheadMs = 30000;
    qint64 offset = request.startMs;
    while (offset < request.durationMs && !control->cancelled.load()) {
        if (offset > control->position.load() + aheadMs) {
            postStatus(generation, QStringLiteral("Субтитры подготовлены до %1").arg(formatMediaTime(offset)));
            while (offset > control->position.load() + aheadMs && !control->cancelled.load())
                QThread::msleep(50);
        }
        if (control->cancelled.load()) return;
        postStatus(generation, QStringLiteral("Чтение аудио · %1").arg(formatMediaTime(offset)));
        const qint64 requestedMs = std::min(chunkMs, request.durationMs - offset);
        auto audio = extractAudio(request.ffmpegPath, request.mediaPath, request.audioTrack,
                                  offset, requestedMs, control->cancelled);
        if (control->cancelled.load()) return;
        if (!audio.error.isEmpty()) {
            postError(generation, audio.error);
            return;
        }
        // Empty audio at the end of a shorter audio track is a processed gap.
        const qint64 end = offset + requestedMs;
        QVector<SubtitleCue> cues;
        double energy = 0;
        for (float value : audio.pcm) energy += double(value) * value;
        const double rms = audio.pcm.isEmpty() ? 0 : std::sqrt(energy / double(audio.pcm.size()));
        if (audio.pcm.size() >= 1600 && rms > 0.0005) {
            postStatus(generation, QStringLiteral("Распознавание английской речи · %1").arg(formatMediaTime(offset)));
            // Pad very short final windows for Whisper, but clamp their timing below.
            const qint64 audioEnd = offset + audio.pcm.size() / 16;
            if (audio.pcm.size() < 16000) audio.pcm.resize(16000);
            QElapsedTimer inferenceTimer;
            inferenceTimer.start();
            if (whisper_full(context.get(), params, audio.pcm.constData(), int(audio.pcm.size())) != 0) {
                if (!control->cancelled.load())
                    postError(generation, QStringLiteral("Whisper не смог обработать аудиофрагмент."));
                return;
            }
            if (control->cancelled.load()) return;
            qInfo() << "Whisper window" << offset << "ms:" << requestedMs
                    << "ms audio processed in" << inferenceTimer.elapsed() << "ms";
            for (int i = 0; i < whisper_full_n_segments(context.get()); ++i) {
                const QString text = QString::fromUtf8(whisper_full_get_segment_text(context.get(), i)).trimmed();
                const qint64 start = std::max(offset, offset + whisper_full_get_segment_t0(context.get(), i) * 10);
                const qint64 finish = std::min({end, audioEnd, offset + whisper_full_get_segment_t1(context.get(), i) * 10});
                if (!text.isEmpty() && finish > start && whisper_full_get_segment_no_speech_prob(context.get(), i) < 0.8f)
                    cues.append({start, finish, text});
            }
        }
        QVector<SubtitleCue> translated;
        if (request.translateToRussian && !cues.isEmpty()) {
            postStatus(generation, QStringLiteral("Перевод EN → RU · %1").arg(formatMediaTime(offset)));
            auto result = translateCues(cues, request.translationModel, control->cancelled);
            if (control->cancelled.load()) return;
            if (!result.error.isEmpty()) {
                postError(generation, result.error);
                return;
            }
            translated = std::move(result.cues);
        }
        QMetaObject::invokeMethod(this, [this, generation, cues, translated, end] {
            if (generation != m_generation) return;
            // Publish translation before advancing the playable interval.
            if (!translated.isEmpty()) emit translatedChunkReady(translated);
            emit chunkReady(cues, end);
        }, Qt::QueuedConnection);
        offset = end;
    }
    if (!control->cancelled.load()) postStatus(generation, request.translateToRussian
        ? QStringLiteral("Распознавание и перевод завершены") : QStringLiteral("Распознавание завершено"));
#endif
}
