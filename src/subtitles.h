#pragma once

#include <QString>
#include <QMetaType>
#include <QVector>

struct SubtitleCue
{
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString text;
};
Q_DECLARE_METATYPE(SubtitleCue)

// An immutable, sorted subtitle timeline. End times are exclusive.
class SubtitleTrack
{
public:
    static SubtitleTrack parseSrt(const QString &text, QString *error);
    static SubtitleTrack fromFile(const QString &path, QString *error);

    const QVector<SubtitleCue> &cues() const { return m_cues; }
    bool isEmpty() const { return m_cues.isEmpty(); }
    QVector<int> activeIndices(qint64 positionMs) const;
    QString textAt(qint64 positionMs) const;
    void append(const QVector<SubtitleCue> &cues);
    QString toSrt() const;

private:
    QVector<SubtitleCue> m_cues;
    QVector<qint64> m_maxEnd;
};

QString formatMediaTime(qint64 milliseconds);
