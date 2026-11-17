#include "translation.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

static TranslationResult translateOne(const QVector<SubtitleCue> &cues, const QString &model,
                                const std::atomic_bool &cancelled, const QUrl &endpoint,
                                int timeoutMs)
{
    TranslationResult result;
    if (cancelled.load() || cues.isEmpty()) return result;
    // Keep even custom/test endpoints strictly on loopback. Never follow redirects.
    if (endpoint.scheme() != "http" || endpoint.host() != "127.0.0.1"
        || !endpoint.userInfo().isEmpty()) {
        result.error = QStringLiteral("Переводчик должен работать локально на 127.0.0.1.");
        return result;
    }
    QJsonArray texts;
    for (const auto &cue : cues) texts.append(cue.text);
    const QJsonObject schema{{"type", "object"},
        {"properties", QJsonObject{{"translations", QJsonObject{{"type", "array"},
            {"items", QJsonObject{{"type", "string"}}},
            {"minItems", cues.size()}, {"maxItems", cues.size()}}}}},
        {"required", QJsonArray{"translations"}}, {"additionalProperties", false}};
    const QJsonObject body{
        {"model", model}, {"stream", false}, {"format", schema}, {"keep_alive", "10m"},
        {"system", "Ты профессиональный переводчик субтитров с английского на русский. "
                   "Переведи текст каждого элемента входного JSON-массива. Верни объект с "
                   "массивом translations. Только перевод, без пояснений, префиксов или "
                   "дополнительных фраз. Не дополняй незавершённые предложения. "
                   "Сохраняй технические термины. Subtitles означает субтитры."},
        {"prompt", QString::fromUtf8(QJsonDocument(texts).toJson(QJsonDocument::Compact))},
        {"options", QJsonObject{{"temperature", 0}, {"num_ctx", 4096}, {"num_predict", 2048}}}
    };
    QNetworkAccessManager network;
    network.setProxy(QNetworkProxy::NoProxy);
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer cancellation, deadline;
    bool timedOut = false, oversized = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
        if (reply->bytesAvailable() > 1024 * 1024) { oversized = true; reply->abort(); }
    });
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] {
        if (cancelled.load()) reply->abort();
    });
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
    });
    cancellation.start(50);
    deadline.start(timeoutMs);
    if (!reply->isFinished()) loop.exec();
    if (cancelled.load()) return result;
    if (timedOut) result.error = QStringLiteral("Перевод не завершён за отведённое время. Проверьте Ollama или выберите меньшую модель.");
    else if (oversized) result.error = QStringLiteral("Переводчик вернул слишком большой ответ.");
    else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404)
        result.error = QStringLiteral("Модель перевода не найдена. Выполните: ollama pull %1").arg(model);
    else if (reply->error() != QNetworkReply::NoError
             || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
        result.error = QStringLiteral("Локальный переводчик недоступен. Запустите Ollama (ollama serve) и загрузите модель %1. %2")
                           .arg(model, reply->errorString());
    if (!result.error.isEmpty()) return result;
    QJsonParseError parseError;
    const auto envelope = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !envelope.isObject()
        || !envelope.object().value("done").toBool()
        || envelope.object().value("done_reason").toString() == "length") {
        result.error = QStringLiteral("Переводчик вернул неполный или некорректный ответ.");
        return result;
    }
    const auto document = QJsonDocument::fromJson(envelope.object().value("response").toString().toUtf8(), &parseError);
    const auto translations = document.object().value("translations");
    if (parseError.error != QJsonParseError::NoError || !translations.isArray()
        || translations.toArray().size() != cues.size()) {
        result.error = QStringLiteral("Переводчик нарушил число реплик. Повторите распознавание или выберите другую модель.");
        return result;
    }
    const auto array = translations.toArray();
    for (qsizetype i = 0; i < cues.size(); ++i) {
        const QString text = array[i].toString().trimmed();
        if (text.isEmpty()) {
            result.cues.clear();
            result.error = QStringLiteral("Переводчик вернул пустую реплику.");
            return result;
        }
        result.cues.append({cues[i].startMs, cues[i].endMs, text});
    }
    return result;
}

TranslationResult translateCues(const QVector<SubtitleCue> &cues, const QString &model,
                                const std::atomic_bool &cancelled, const QUrl &endpoint,
                                int timeoutMs)
{
    TranslationResult result;
    // Small models can merge adjacent meanings despite correct array length.
    // One cue per request preserves boundaries without trusting generated IDs.
    for (const auto &cue : cues) {
        if (cancelled.load()) return {};
        auto item = translateOne({cue}, model, cancelled, endpoint, timeoutMs);
        if (cancelled.load()) return {};
        if (!item.error.isEmpty()) return {{}, item.error};
        result.cues.append(item.cues);
    }
    return result;
}
