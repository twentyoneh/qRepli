#pragma once

#include "subtitles.h"
#include <QAbstractListModel>

class SubtitleModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
public:
    enum Role { TextRole = Qt::UserRole + 1, StartMsRole, TimeLabelRole, ActiveRole };
    explicit SubtitleModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_track.cues().size()); }
    int currentIndex() const { return m_currentIndex; }
    void setTrack(const SubtitleTrack &track);
    void setPosition(qint64 position);

signals:
    void countChanged();
    void currentIndexChanged();

private:
    SubtitleTrack m_track;
    QVector<int> m_active;
    int m_currentIndex = -1;
};
