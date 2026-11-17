#include "subtitles.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextDocumentFragment>
#include <algorithm>

namespace {
constexpr qint64 maxFileSize = 8 * 1024 * 1024;

qint64 timestamp(const QRegularExpressionMatch &match, int group)
{
    return ((match.captured(group).toLongLong() * 60
             + match.captured(group + 1).toLongLong()) * 60
            + match.captured(group + 2).toLongLong()) * 1000
           + match.captured(group + 3).toLongLong();
}
}

SubtitleTrack SubtitleTrack::fromFile(const QString &path, QString *error)
{
    error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("SubtitleTrack", "Не удалось открыть субтитры: %1").arg(file.errorString());
        return {};
    }
    if (file.size() > maxFileSize) {
        *error = QCoreApplication::translate("SubtitleTrack", "Файл субтитров слишком большой. Максимум — 8 МБ.");
        return {};
    }
    const QByteArray bytes = file.read(maxFileSize + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maxFileSize) {
        *error = QCoreApplication::translate("SubtitleTrack", "Не удалось прочитать файл субтитров.");
        return {};
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        *error = QCoreApplication::translate("SubtitleTrack", "Сохраните SRT в кодировке UTF-8 и откройте его снова.");
        return {};
    }
    return parseSrt(text, error);
}

SubtitleTrack SubtitleTrack::parseSrt(const QString &input, QString *error)
{
    error->clear();
    QString text = input;
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');

    static const QRegularExpression separator(QStringLiteral("\\n[\\t ]*\\n"));
    static const QRegularExpression number(QStringLiteral("^\\d+$"));
    static const QRegularExpression timing(QStringLiteral(
        "^(\\d{1,3}):([0-5]\\d):([0-5]\\d)[,.](\\d{3})\\s+-->\\s+"
        "(\\d{1,3}):([0-5]\\d):([0-5]\\d)[,.](\\d{3})(?:\\s+.*)?$"));
    SubtitleTrack result;
    const auto blocks = text.trimmed().split(separator, Qt::SkipEmptyParts);
    int blockNumber = 0;
    for (const auto &block : blocks) {
        ++blockNumber;
        QStringList lines = block.trimmed().split(u'\n');
        if (lines.isEmpty())
            continue;
        if (number.match(lines.first().trimmed()).hasMatch())
            lines.removeFirst();
        if (lines.size() < 2) {
            *error = QCoreApplication::translate("SubtitleTrack", "SRT: нет текста или времени в блоке %1.").arg(blockNumber);
            return {};
        }
        const auto match = timing.match(lines.takeFirst().trimmed());
        if (!match.hasMatch()) {
            *error = QCoreApplication::translate("SubtitleTrack", "SRT: неверное время в блоке %1.").arg(blockNumber);
            return {};
        }
        SubtitleCue cue{timestamp(match, 1), timestamp(match, 5), lines.join(u'\n').trimmed()};
        if (cue.endMs <= cue.startMs) {
            *error = QCoreApplication::translate("SubtitleTrack", "SRT: конец реплики должен быть позже начала (блок %1).")
                         .arg(blockNumber);
            return {};
        }
        // Keep line breaks while flattening SRT's optional HTML formatting.
        // The resulting text is always rendered as plain text in QML.
        cue.text = QTextDocumentFragment::fromHtml(
                       QString(cue.text).replace(u'\n', QStringLiteral("<br>"))).toPlainText().trimmed();
        if (cue.text.isEmpty()) {
            *error = QCoreApplication::translate("SubtitleTrack", "SRT: пустая реплика в блоке %1.").arg(blockNumber);
            return {};
        }
        result.m_cues.append(std::move(cue));
    }
    if (result.isEmpty()) {
        *error = QCoreApplication::translate("SubtitleTrack", "В файле не найдены реплики SRT.");
        return {};
    }
    std::stable_sort(result.m_cues.begin(), result.m_cues.end(),
                     [](const auto &a, const auto &b) { return a.startMs < b.startMs; });
    qint64 maxEnd = 0;
    for (const auto &cue : result.m_cues) {
        maxEnd = std::max(maxEnd, cue.endMs);
        result.m_maxEnd.append(maxEnd);
    }
    return result;
}

QVector<int> SubtitleTrack::activeIndices(qint64 positionMs) const
{
    QVector<int> result;
    auto after = std::upper_bound(m_cues.cbegin(), m_cues.cend(), positionMs,
                                  [](qint64 time, const auto &cue) { return time < cue.startMs; });
    for (int i = int(after - m_cues.cbegin()) - 1; i >= 0 && m_maxEnd[i] > positionMs; --i) {
        if (m_cues[i].endMs > positionMs)
            result.append(i);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

QString SubtitleTrack::textAt(qint64 positionMs) const
{
    QStringList text;
    for (int index : activeIndices(positionMs))
        text.append(m_cues[index].text);
    return text.join(u'\n');
}

QString formatMediaTime(qint64 milliseconds)
{
    const qint64 seconds = std::max(qint64(0), milliseconds) / 1000;
    const QString tail = QStringLiteral("%1:%2")
                             .arg((seconds / 60) % 60, 2, 10, QChar(u'0'))
                             .arg(seconds % 60, 2, 10, QChar(u'0'));
    return seconds >= 3600 ? QStringLiteral("%1:%2").arg(seconds / 3600).arg(tail) : tail;
}

void SubtitleTrack::append(const QVector<SubtitleCue> &cues)
{
    for (const auto &cue : cues) {
        if (cue.startMs >= 0 && cue.endMs > cue.startMs && !cue.text.isEmpty())
            m_cues.append(cue);
    }
    std::stable_sort(m_cues.begin(), m_cues.end(),
                     [](const auto &a, const auto &b) { return a.startMs < b.startMs; });
    m_maxEnd.clear();
    qint64 end = 0;
    for (const auto &cue : m_cues) {
        end = std::max(end, cue.endMs);
        m_maxEnd.append(end);
    }
}

QString SubtitleTrack::toSrt() const
{
    const auto stamp = [](qint64 ms) {
        return QStringLiteral("%1:%2:%3,%4")
            .arg(ms / 3600000, 2, 10, QChar(u'0'))
            .arg((ms / 60000) % 60, 2, 10, QChar(u'0'))
            .arg((ms / 1000) % 60, 2, 10, QChar(u'0'))
            .arg(ms % 1000, 3, 10, QChar(u'0'));
    };
    QString output;
    int index = 1;
    for (const auto &cue : m_cues)
        output += QStringLiteral("%1\n%2 --> %3\n%4\n\n")
                      .arg(index++).arg(stamp(cue.startMs), stamp(cue.endMs), cue.text.toHtmlEscaped());
    return output;
}
