#pragma once

#include "subtitlemodel.h"
#include "transcription.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <QTimer>

class QVideoSink;

class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY sourceChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY sourceChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool playRequested READ playRequested NOTIFY stateChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString originalText READ originalText NOTIFY subtitleTextChanged)
    Q_PROPERTY(QString translatedText READ translatedText NOTIFY subtitleTextChanged)
    Q_PROPERTY(QString originalFile READ originalFile NOTIFY subtitleFilesChanged)
    Q_PROPERTY(QString translatedFile READ translatedFile NOTIFY subtitleFilesChanged)
    Q_PROPERTY(SubtitleModel* transcript READ transcript CONSTANT)
    Q_PROPERTY(QStringList audioTracks READ audioTracks NOTIFY audioTracksChanged)
    Q_PROPERTY(int audioTrack READ audioTrack WRITE setAudioTrack NOTIFY audioTracksChanged)
    Q_PROPERTY(bool asrAvailable READ asrAvailable CONSTANT)
    Q_PROPERTY(bool asrRunning READ asrRunning NOTIFY asrChanged)
    Q_PROPERTY(QString asrStatus READ asrStatus NOTIFY asrChanged)
    Q_PROPERTY(QString modelPath READ modelPath NOTIFY asrSettingsChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath WRITE setFfmpegPath NOTIFY asrSettingsChanged)
    Q_PROPERTY(bool translateToRussian READ translateToRussian WRITE setTranslateToRussian NOTIFY asrSettingsChanged)
    Q_PROPERTY(QString translationModel READ translationModel WRITE setTranslationModel NOTIFY asrSettingsChanged)
    Q_PROPERTY(bool useGpu READ useGpu WRITE setUseGpu NOTIFY asrSettingsChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;
    QString fileName() const;
    bool hasMedia() const { return !m_source.isEmpty(); }
    qint64 position() const { return m_player.position(); }
    qint64 duration() const { return m_player.duration(); }
    bool playing() const { return m_player.playbackState() == QMediaPlayer::PlayingState; }
    bool playRequested() const { return m_playRequested; }
    bool seekable() const { return m_player.isSeekable(); }
    bool busy() const;
    QString status() const;
    double volume() const { return m_audio.volume(); }
    void setVolume(double volume);
    qreal playbackRate() const { return m_player.playbackRate(); }
    void setPlaybackRate(qreal rate);
    QString error() const { return m_error; }
    QString originalText() const { return m_originalText; }
    QString translatedText() const { return m_translatedText; }
    QString originalFile() const { return m_generatedOriginal ? tr("Распознанная речь · EN") : m_originalFile; }
    QString translatedFile() const { return m_generatedTranslation ? tr("Автоперевод · RU") : m_translatedFile; }
    SubtitleModel *transcript() { return &m_transcript; }
    QStringList audioTracks() const;
    int audioTrack() const { return m_player.activeAudioTrack(); }
    void setAudioTrack(int track);
    bool asrAvailable() const { return TranscriptionService::available(); }
    bool asrRunning() const { return m_asr.running() || m_asrRestart.isActive(); }
    QString asrStatus() const { return m_asr.status(); }
    QString modelPath() const { return m_modelPath; }
    QString ffmpegPath() const { return m_ffmpegPath; }
    bool useGpu() const { return m_useGpu; }
    void setFfmpegPath(const QString &path);
    void setUseGpu(bool value);
    bool translateToRussian() const { return m_translateToRussian; }
    QString translationModel() const { return m_translationModel; }
    void setTranslateToRussian(bool enabled);
    void setTranslationModel(const QString &model);

    Q_INVOKABLE bool openMedia(const QUrl &url);
    Q_INVOKABLE bool openSubtitles(const QUrl &url, bool translation = false);
    Q_INVOKABLE void clearSubtitles();
    Q_INVOKABLE void setVideoSink(QVideoSink *sink);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void seek(qint64 position);
    Q_INVOKABLE void skip(qint64 offset);
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void setWhisperModel(const QUrl &url);
    Q_INVOKABLE void startTranscription();
    Q_INVOKABLE void stopTranscription();
    Q_INVOKABLE bool saveOriginal(const QUrl &url);
    Q_INVOKABLE QString timeLabel(qint64 time) const { return formatMediaTime(time); }

signals:
    void sourceChanged();
    void positionChanged();
    void durationChanged();
    void stateChanged();
    void volumeChanged();
    void playbackRateChanged();
    void errorChanged();
    void subtitleTextChanged();
    void subtitleFilesChanged();
    void audioTracksChanged();
    void asrChanged();
    void asrSettingsChanged();

private:
    void updateSubtitles();
    void setError(const QString &message);
    void restartTranscription();
    void updatePlayback();
    bool waitingForAsr() const;

    QAudioOutput m_audio;
    QMediaPlayer m_player;
    SubtitleTrack m_original;
    SubtitleTrack m_translated;
    SubtitleModel m_transcript;
    QUrl m_source;
    QString m_error;
    QString m_originalText;
    QString m_translatedText;
    QString m_originalFile;
    QString m_translatedFile;
    TranscriptionService m_asr;
    QTimer m_asrRestart;
    QString m_modelPath;
    QString m_ffmpegPath;
    bool m_useGpu = true;
    bool m_translateToRussian = true;
    bool m_generatedTranslation = false;
    bool m_generatedOriginal = false;
    QString m_translationModel;
    bool m_generated = false;
    bool m_playRequested = false;
    qint64 m_processedStart = 0;
    qint64 m_processedUntil = 0;
};
