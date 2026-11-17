#include "subtitles.h"
#include "subtitlemodel.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class SubtitleTests : public QObject
{
    Q_OBJECT
private slots:
    void parseFormattingAndBom()
    {
        QString error;
        const auto track = SubtitleTrack::parseSrt(
            QString(QChar(0xfeff)) + "1\r\n00:00:01,250 --> 00:00:03,500\r\n"
                                   "<i>Hello &amp; welcome</i>\r\nПривет!\r\n", &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(track.cues().size(), 1);
        QCOMPARE(track.cues()[0].startMs, 1250);
        QCOMPARE(track.cues()[0].endMs, 3500);
        QCOMPARE(track.cues()[0].text, QString("Hello & welcome\nПривет!"));
    }

    void sortedTimelineAndOverlaps()
    {
        QString error;
        const auto track = SubtitleTrack::parseSrt(
            "3\n00:00:03,000 --> 00:00:04,000\nC\n\n"
            "1\n00:00:01,000 --> 00:00:06,000\nA\n\n"
            "2\n00:00:02,000 --> 00:00:03,000\nB\n", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(track.textAt(999), QString());
        QCOMPARE(track.textAt(1000), QString("A"));
        QCOMPARE(track.textAt(2500), QString("A\nB"));
        QCOMPARE(track.textAt(3000), QString("A\nC"));
        QCOMPARE(track.textAt(4000), QString("A"));
        QCOMPARE(track.textAt(6000), QString());
        // Seeking backwards must not retain a later cue.
        QCOMPARE(track.textAt(2000), QString("A\nB"));
    }

    void invalid_data()
    {
        QTest::addColumn<QString>("srt");
        QTest::newRow("empty") << QString();
        QTest::newRow("not-srt") << QString("hello");
        QTest::newRow("minutes") << QString("1\n00:60:00,000 --> 01:01:00,000\nWrong");
        QTest::newRow("reverse") << QString("1\n00:00:02,000 --> 00:00:01,000\nWrong");
        QTest::newRow("zero-length") << QString("1\n00:00:02,000 --> 00:00:02,000\nWrong");
        QTest::newRow("missing-text") << QString("1\n00:00:01,000 --> 00:00:02,000\n");
        QTest::newRow("broken-second-block") << QString("1\n00:00:01,000 --> 00:00:02,000\nGood\n\n2\nOops\nBad");
    }

    void invalid()
    {
        QFETCH(QString, srt);
        QString error;
        QVERIFY(SubtitleTrack::parseSrt(srt, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rejectsInvalidEncoding()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath("bad.srt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xfe\xc0", 3);
        file.close();
        QString error;
        QVERIFY(SubtitleTrack::fromFile(file.fileName(), &error).isEmpty());
        QVERIFY(error.contains("UTF-8"));
    }

    void modelFollowsPositionAndReset()
    {
        QString error;
        SubtitleModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setTrack(SubtitleTrack::parseSrt("1\n00:00:01,000 --> 00:00:02,000\nHello", &error));
        QCOMPARE(model.count(), 1);
        model.setPosition(1000);
        QCOMPARE(model.currentIndex(), 0);
        QVERIFY(model.data(model.index(0), SubtitleModel::ActiveRole).toBool());
        model.setPosition(2000);
        QCOMPARE(model.currentIndex(), -1);
        model.setTrack({});
        QCOMPARE(model.count(), 0);
        QCOMPARE(model.currentIndex(), -1);
    }

    void formatsTime()
    {
        QCOMPARE(formatMediaTime(-100), QString("00:00"));
        QCOMPARE(formatMediaTime(61000), QString("01:01"));
        QCOMPARE(formatMediaTime(3661000), QString("1:01:01"));
    }

    void generatedCuesCanBeExported()
    {
        SubtitleTrack track;
        track.append({{1200, 2500, "Hello & welcome\nLine two"}, {0, 0, "invalid"}});
        track.append({{3000, 4200, "Next sentence"}});
        QString error;
        const auto restored = SubtitleTrack::parseSrt(track.toSrt(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(restored.cues().size(), 2);
        QCOMPARE(restored.textAt(1200), QString("Hello & welcome\nLine two"));
        QCOMPARE(restored.cues()[1].startMs, 3000);
    }
};

QTEST_MAIN(SubtitleTests)
#include "test_subtitles.moc"
