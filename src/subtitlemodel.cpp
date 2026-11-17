#include "subtitlemodel.h"

int SubtitleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant SubtitleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const auto &cue = m_track.cues()[index.row()];
    switch (role) {
    case TextRole: return cue.text;
    case StartMsRole: return cue.startMs;
    case TimeLabelRole: return formatMediaTime(cue.startMs);
    case ActiveRole: return m_active.contains(index.row());
    default: return {};
    }
}

QHash<int, QByteArray> SubtitleModel::roleNames() const
{
    return {{TextRole, "cueText"}, {StartMsRole, "startMs"},
            {TimeLabelRole, "timeLabel"}, {ActiveRole, "active"}};
}

void SubtitleModel::setTrack(const SubtitleTrack &track)
{
    beginResetModel();
    m_track = track;
    m_active.clear();
    m_currentIndex = -1;
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}

void SubtitleModel::setPosition(qint64 position)
{
    const auto active = m_track.activeIndices(position);
    if (active == m_active)
        return;
    auto changed = m_active;
    for (int row : active) {
        if (!changed.contains(row))
            changed.append(row);
    }
    m_active = active;
    const int currentIndex = active.isEmpty() ? -1 : active.last();
    for (int row : changed)
        emit dataChanged(index(row), index(row), {ActiveRole});
    if (currentIndex != m_currentIndex) {
        m_currentIndex = currentIndex;
        emit currentIndexChanged();
    }
}
