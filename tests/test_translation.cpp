#include "translation.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QtTest>

class TranslationTests : public QObject
{
    Q_OBJECT
private:
    QVector<SubtitleCue> cues{{1000, 2400, "Hello."}, {2500, 5000, "Play the video."}};
    QByteArray response(const QJsonArray &items) {
        return QJsonDocument(QJsonObject{{"done", true}, {"done_reason", "stop"},
            {"response", QString::fromUtf8(QJsonDocument(QJsonObject{{"translations", items}})
                                              .toJson(QJsonDocument::Compact))}}).toJson();
    }
    TranslationResult run(const QByteArray &body, int code = 200, bool cancel = false,
                          bool stall = false, int timeout = 1000) {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost)) return {{}, "Cannot listen"};
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer] {
                buffer->append(socket->readAll());
                const auto split = buffer->indexOf("\r\n\r\n");
                if (split < 0 || socket->property("handled").toBool()) return;
                const auto headers = buffer->left(split).split('\n');
                int size = 0;
                for (auto header : headers)
                    if (header.toLower().startsWith("content-length:")) size = header.mid(15).trimmed().toInt();
                if (buffer->size() < split + 4 + size) return;
                socket->setProperty("handled", true);
                const auto request = QJsonDocument::fromJson(buffer->mid(split + 4, size)).object();
                QCOMPARE(request.value("stream").toBool(true), false);
                QCOMPARE(request.value("model").toString(), QString("test-model"));
                QVERIFY(request.value("format").isObject());
                if (!stall) {
                    socket->write("HTTP/1.1 " + QByteArray::number(code) + " Result\r\nContent-Type: application/json\r\nContent-Length: "
                                  + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                }
            });
        });
        std::atomic_bool cancelled{false};
        TranslationResult result;
        const QUrl endpoint(QString("http://127.0.0.1:%1/api/generate").arg(server.serverPort()));
        auto thread = std::unique_ptr<QThread>(QThread::create([&] {
            result = translateCues(cues, "test-model", cancelled, endpoint, timeout);
        }));
        QEventLoop loop;
        connect(thread.get(), &QThread::finished, &loop, &QEventLoop::quit);
        if (cancel) QTimer::singleShot(100, &loop, [&] { cancelled.store(true); });
        thread->start();
        loop.exec();
        thread->wait();
        return result;
    }
private slots:
    void preservesCueOrderAndTimestamps() {
        auto result = run(response({"Привет."}));
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.cues.size(), 2);
        QCOMPARE(result.cues[0].text, QString("Привет."));
        QCOMPARE(result.cues[0].startMs, 1000);
        QCOMPARE(result.cues[1].endMs, 5000);
    }
    void invalidResponse_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("bad-json") << QByteArray("not json");
        QTest::newRow("wrong-count") << response({"Привет.", "Лишняя реплика."});
        QTest::newRow("empty-cue") << response({"  "});
        QTest::newRow("wrong-type") << response({123});
        QTest::newRow("unfinished") << QByteArray("{\"done\":false}");
    }
    void invalidResponse() {
        QFETCH(QByteArray, body);
        auto result = run(body);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.cues.isEmpty());
    }
    void missingModel() {
        auto result = run("{}", 404);
        QVERIFY(result.error.contains("ollama pull test-model"));
    }
    void timeout() {
        auto result = run({}, 200, false, true, 100);
        QVERIFY(result.error.contains("время"));
        QVERIFY(result.cues.isEmpty());
    }
    void cancellation() {
        auto result = run({}, 200, true, true);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.cues.isEmpty());
    }
    void silenceAndRemoteEndpoint() {
        std::atomic_bool cancelled{false};
        QVERIFY(translateCues({}, "unused", cancelled).error.isEmpty());
        QVERIFY(!translateCues(cues, "unused", cancelled, QUrl("https://example.com")).error.isEmpty());
    }
    void realModel() {
        if (!qEnvironmentVariableIsSet("QREPLI_TEST_TRANSLATION")) QSKIP("Set QREPLI_TEST_TRANSLATION for local Ollama integration");
        std::atomic_bool cancelled{false};
        TranslationResult result;
        auto thread = std::unique_ptr<QThread>(QThread::create([&] {
            result = translateCues(cues, "qwen2.5:3b", cancelled);
        }));
        QEventLoop loop;
        connect(thread.get(), &QThread::finished, &loop, &QEventLoop::quit);
        thread->start(); loop.exec(); thread->wait();
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.cues.size(), 2);
        QCOMPARE(result.cues[1].startMs, cues[1].startMs);
        QVERIFY(result.cues[1].text.contains(QRegularExpression("[А-Яа-яЁё]")));
        for (const auto &cue : result.cues) qInfo().noquote() << cue.text;
    }
};
QTEST_GUILESS_MAIN(TranslationTests)
#include "test_translation.moc"
