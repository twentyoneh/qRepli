#include "playercontroller.h"

#include <QFileInfo>
#include <QLocale>
#include <QMediaMetaData>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QSaveFile>
#include <QVideoSink>
#include <algorithm>
#include <cmath>

PlayerController::PlayerController(QObject *parent) : QObject(parent)
{
    m_player.setAudioOutput(&m_audio);
    m_audio.setVolume(float(std::clamp(QSettings().value("volume", 0.7).toDouble(), 0.0, 1.0)));
    m_modelPath = QSettings().value("asr/model").toString();
    m_useGpu = QSettings().value("asr/useGpu", true).toBool();
    m_ffmpegPath = QSettings().value("asr/ffmpeg").toString();
    if (m_ffmpegPath.isEmpty()) {
        m_ffmpegPath = QStandardPaths::findExecutable("ffmpeg");
#ifdef Q_OS_MACOS
        if (m_ffmpegPath.isEmpty())
            m_ffmpegPath = QStandardPaths::findExecutable("ffmpeg", {"/opt/homebrew/bin", "/usr/local/bin"});
#endif
    }
    if (m_modelPath.isEmpty()) {
        for (const QString &base : {QDir::currentPath(), QCoreApplication::applicationDirPath()}) {
            const QString path = QDir(base).filePath("models/ggml-base.en.bin");
            if (QFileInfo::exists(path)) { m_modelPath = path; break; }
        }
    }
    m_asrRestart.setSingleShot(true);
    m_asrRestart.setInterval(250);
    connect(&m_asrRestart, &QTimer::timeout, this, &PlayerController::restartTranscription);
    connect(&m_asr, &TranscriptionService::stateChanged, this, [this] {
        emit asrChanged();
        emit stateChanged();
    });
    connect(&m_asr, &TranscriptionService::chunkReady, this,
            [this](const QVector<SubtitleCue> &cues, qint64 until) {
                m_original.append(cues);
                m_processedUntil = until;
                m_transcript.setTrack(m_original);
                updateSubtitles();
                updatePlayback();
                emit asrChanged();
            });
    connect(&m_asr, &TranscriptionService::failed, this, [this](const QString &error) {
        m_generated = false;
        m_playRequested = false;
        m_player.pause();
        setError(error);
        emit stateChanged();
    });
    connect(&m_player, &QMediaPlayer::positionChanged, this, [this] {
        m_asr.setPosition(position());
        updateSubtitles();
        if (waitingForAsr() && playing()) m_player.pause();
        emit positionChanged();
    });
    connect(&m_player, &QMediaPlayer::durationChanged, this, &PlayerController::durationChanged);
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &PlayerController::stateChanged);
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
            m_playRequested = false;
        emit stateChanged();
    });
    connect(&m_player, &QMediaPlayer::seekableChanged, this, &PlayerController::stateChanged);
    connect(&m_player, &QMediaPlayer::tracksChanged, this, &PlayerController::audioTracksChanged);
    connect(&m_player, &QMediaPlayer::activeTracksChanged, this, &PlayerController::audioTracksChanged);
    connect(&m_audio, &QAudioOutput::volumeChanged, this, &PlayerController::volumeChanged);
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &detail) {
                setError(QStringLiteral("Не удалось воспроизвести файл. %1").arg(detail));
                emit stateChanged();
            });
}

PlayerController::~PlayerController()
{
    m_asr.shutdown();
    m_player.stop();
}

QString PlayerController::fileName() const { return QFileInfo(m_source.toLocalFile()).fileName(); }

bool PlayerController::busy() const
{
    return m_player.mediaStatus() == QMediaPlayer::LoadingMedia
           || m_player.mediaStatus() == QMediaPlayer::StalledMedia;
}

QString PlayerController::status() const
{
    if (!hasMedia()) return QStringLiteral("Откройте видео");
    if (m_player.mediaStatus() == QMediaPlayer::InvalidMedia) return QStringLiteral("Ошибка файла");
    if (m_playRequested && waitingForAsr()) return QStringLiteral("Подготовка субтитров…");
    if (busy()) return QStringLiteral("Загрузка…");
    if (m_player.mediaStatus() == QMediaPlayer::EndOfMedia) return QStringLiteral("Просмотр завершён");
    return playing() ? QStringLiteral("Воспроизведение") : QStringLiteral("Пауза");
}

void PlayerController::setVolume(double volume)
{
    if (!std::isfinite(volume)) return;
    const double clamped = std::clamp(volume, 0.0, 1.0);
    m_audio.setVolume(float(clamped));
    QSettings().setValue("volume", clamped);
}

bool PlayerController::openMedia(const QUrl &url)
{
    const QFileInfo file(url.toLocalFile());
    if (!url.isLocalFile() || !file.isFile() || !file.isReadable()) {
        setError(QStringLiteral("Выберите доступный локальный видеофайл."));
        return false;
    }
    m_playRequested = false;
    stopTranscription();
    m_player.stop();
    clearSubtitles();
    dismissError();
    m_source = url;
    m_player.setSource(url);
    emit sourceChanged();
    m_playRequested = true;
    m_player.play();
    return true;
}

bool PlayerController::openSubtitles(const QUrl &url, bool translation)
{
    if (!hasMedia()) {
        setError(QStringLiteral("Сначала откройте видео."));
        return false;
    }
    if (!url.isLocalFile()) {
        setError(QStringLiteral("Выберите локальный файл SRT."));
        return false;
    }
    QString error;
    SubtitleTrack track = SubtitleTrack::fromFile(url.toLocalFile(), &error);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    if (translation) {
        m_translated = std::move(track);
        m_translatedFile = QFileInfo(url.toLocalFile()).fileName();
    } else {
        stopTranscription();
        m_original = std::move(track);
        m_originalFile = QFileInfo(url.toLocalFile()).fileName();
    }
    m_transcript.setTrack(m_original.isEmpty() ? m_translated : m_original);
    updateSubtitles();
    emit subtitleFilesChanged();
    dismissError();
    return true;
}

void PlayerController::clearSubtitles()
{
    stopTranscription();
    m_original = {};
    m_translated = {};
    m_originalFile.clear();
    m_translatedFile.clear();
    m_transcript.setTrack({});
    updateSubtitles();
    emit subtitleFilesChanged();
}

void PlayerController::updateSubtitles()
{
    const auto original = m_original.textAt(position());
    const auto translated = m_translated.textAt(position());
    if (original != m_originalText || translated != m_translatedText) {
        m_originalText = original;
        m_translatedText = translated;
        emit subtitleTextChanged();
    }
    m_transcript.setPosition(position());
}

void PlayerController::setVideoSink(QVideoSink *sink) { m_player.setVideoSink(sink); }

void PlayerController::togglePlayback()
{
    if (!hasMedia()) return;
    m_playRequested = !m_playRequested;
    if (m_playRequested && m_player.mediaStatus() == QMediaPlayer::EndOfMedia) {
        seek(0);
    }
    updatePlayback();
}

void PlayerController::seek(qint64 value)
{
    if (!seekable()) return;
    const qint64 target = std::clamp(value, qint64(0), duration());
    if (m_generated && (target < m_processedStart || target >= m_processedUntil)) {
        m_asr.cancel();
        m_original = {};
        m_transcript.setTrack({});
        m_processedStart = target;
        m_processedUntil = target;
        m_asrRestart.start();
        emit asrChanged();
    }
    m_player.setPosition(target);
    m_asr.setPosition(target);
    updateSubtitles();
    updatePlayback();
}

void PlayerController::skip(qint64 offset) { seek(position() + offset); }

QStringList PlayerController::audioTracks() const
{
    QStringList names;
    const auto tracks = m_player.audioTracks();
    for (int i = 0; i < tracks.size(); ++i) {
        QString description = tracks[i].stringValue(QMediaMetaData::Title);
        const QString language = tracks[i].stringValue(QMediaMetaData::Language);
        if (description.isEmpty()) description = language;
        names.append(description.isEmpty()
                         ? QStringLiteral("Дорожка %1").arg(i + 1)
                         : QStringLiteral("%1 · %2").arg(i + 1).arg(description));
    }
    return names;
}

void PlayerController::setAudioTrack(int track)
{
    if (track >= 0 && track < m_player.audioTracks().size() && track != audioTrack()) {
        const bool restart = m_generated;
        stopTranscription();
        m_player.setActiveAudioTrack(track);
        if (restart) startTranscription();
    }
}

void PlayerController::setError(const QString &message)
{
    if (m_error == message) return;
    m_error = message;
    emit errorChanged();
}

void PlayerController::dismissError() { setError({}); }

bool PlayerController::waitingForAsr() const
{
    return m_generated && position() < duration()
           && (position() < m_processedStart || position() >= m_processedUntil);
}

void PlayerController::updatePlayback()
{
    if (!m_playRequested || waitingForAsr()) m_player.pause();
    else if (hasMedia() && !playing()) m_player.play();
    emit stateChanged();
}

void PlayerController::setWhisperModel(const QUrl &url)
{
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) {
        setError(QStringLiteral("Выберите локальный файл модели Whisper (.bin)."));
        return;
    }
    stopTranscription();
    m_modelPath = url.toLocalFile();
    QSettings().setValue("asr/model", m_modelPath);
    emit asrSettingsChanged();
}

void PlayerController::setFfmpegPath(const QString &path)
{
    stopTranscription();
    m_ffmpegPath = path.trimmed();
    QSettings().setValue("asr/ffmpeg", m_ffmpegPath);
    emit asrSettingsChanged();
}

void PlayerController::setUseGpu(bool value)
{
    if (m_useGpu == value) return;
    stopTranscription();
    m_useGpu = value;
    QSettings().setValue("asr/useGpu", value);
    emit asrSettingsChanged();
}

void PlayerController::startTranscription()
{
    if (!asrAvailable()) {
        setError(QStringLiteral("Эта сборка создана без Whisper."));
        return;
    }
    if (!hasMedia() || !seekable() || duration() <= 0 || audioTrack() < 0) {
        setError(QStringLiteral("Откройте видео с аудиодорожкой и дождитесь загрузки."));
        return;
    }
    if (!QFileInfo(m_modelPath).isFile()) {
        setError(QStringLiteral("Сначала выберите модель Whisper в настройках распознавания."));
        return;
    }
    if (QStandardPaths::findExecutable(m_ffmpegPath).isEmpty()) {
        setError(QStringLiteral("FFmpeg не найден. Укажите путь к нему в настройках распознавания."));
        return;
    }
    m_generated = true;
    if (position() >= duration()) m_player.setPosition(0);
    dismissError();
    restartTranscription();
}

void PlayerController::restartTranscription()
{
    if (!m_generated) return;
    m_original = {};
    m_originalFile = QStringLiteral("Распознанная речь · EN");
    m_transcript.setTrack({});
    // A little preceding audio gives Whisper context after seeking and ensures
    // a run started just after Play also covers time zero when replaying.
    const qint64 start = std::max<qint64>(0, position() - 1000);
    m_processedStart = start;
    m_processedUntil = start;
    m_asr.start({m_source.toLocalFile(), m_modelPath,
                 QStandardPaths::findExecutable(m_ffmpegPath), audioTrack(),
                 start, duration(), m_useGpu});
    m_asr.setPosition(position());
    updateSubtitles();
    updatePlayback();
    emit subtitleFilesChanged();
}

void PlayerController::stopTranscription()
{
    m_asrRestart.stop();
    m_generated = false;
    m_asr.cancel();
    updatePlayback();
    emit asrChanged();
}

bool PlayerController::saveOriginal(const QUrl &url)
{
    if (!url.isLocalFile() || m_original.isEmpty()) {
        setError(QStringLiteral("Нет оригинальных субтитров для сохранения."));
        return false;
    }
    // Never allow an export to overwrite the currently opened movie or model.
    const QString destination = QFileInfo(url.toLocalFile()).absoluteFilePath();
    if (QFileInfo(destination) == QFileInfo(m_source.toLocalFile())
        || QFileInfo(destination) == QFileInfo(m_modelPath)) {
        setError(QStringLiteral("Для субтитров выберите отдельный файл SRT."));
        return false;
    }
    QSaveFile file(destination);
    const QByteArray bytes = m_original.toSrt().toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(QStringLiteral("Не удалось сохранить субтитры: %1").arg(file.errorString()));
        return false;
    }
    dismissError();
    return true;
}
