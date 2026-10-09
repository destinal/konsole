// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-FileCopyrightText: George Nachman and the iTerm2 authors
// SPDX-License-Identifier: GPL-2.0-only
//
// Ported from iTerm2's ProgramStatusTests.

// Ports the record-model tests of iTerm2 (ProgramStatusTests) and checks how
// OSC 7501 records share a session's status with the hooks.

#include "programstatusrecords.h"
#include "sessionstatus.h"

#include <QTest>

using Konsole::ProgramStatusReport;

namespace
{
ProgramStatusReport report(const char *body)
{
    return *ProgramStatusReport::parse(QString::fromUtf8(body));
}

QString text(const SessionStatus &status)
{
    return status.statusText().value_or(QStringLiteral("<none>"));
}

QString detail(const SessionStatus &status)
{
    return status.detail().value_or(QStringLiteral("<none>"));
}

// base64 of "Pushing image", "US East", "Push failed".
const char *const pushing = "UHVzaGluZyBpbWFnZQ";
}

class ProgramStatusRecordsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testReportReplacesItsRecord();
    void testClearRemovesDescendantsOnly();
    void testClearWithoutIdRemovesEverything();
    void testCapacityEvictsTheOldest();
    void testMostUrgentWins();
    void testAppIsInherited();
    void testMarkSeenKeepsText();

    void testReportSetsTheStatus();
    void testBlockedWithoutMessageDescribesTheKind();
    void testChildTitlePrefixesTheDetail();
    void testReportWithoutMessageClearsTheDetail();
    void testClearingTheLastRecordClearsTheStatus();
    void testReportSupersedesAHookExpiration();
    void testOtherWriterKeepsTheStatus();
    void testPromptEndsTransientRecords();
    void testKeyPressMarksFinishedRecordsSeen();
    void testTerminalResetRemovesRecords();
    void testSanitizedText();
};

void ProgramStatusRecordsTest::testReportReplacesItsRecord()
{
    ProgramStatusRecords records;
    records.apply(report("state=working:app=brew:progress=30"));
    records.apply(report("state=blocked:kind=auth"));
    const auto record = records.record({});
    QCOMPARE(record->report.state, ProgramStatusReport::Blocked);
    QVERIFY(!record->report.app.has_value());
    QVERIFY(!record->report.progress.has_value());
    QCOMPARE(records.size(), 1);
}

void ProgramStatusRecordsTest::testClearRemovesDescendantsOnly()
{
    ProgramStatusRecords records;
    records.apply(report("state=working:id=a"));
    records.apply(report("state=working:id=a/b"));
    records.apply(report("state=working:id=a/b/c"));
    records.apply(report("state=working:id=ab"));
    records.apply(report("state=working"));
    records.apply(report("state=clear:id=a"));
    QCOMPARE(records.size(), 2);
    QVERIFY(records.record({QStringLiteral("ab")}).has_value());
    QVERIFY(records.record({}).has_value());
}

void ProgramStatusRecordsTest::testClearWithoutIdRemovesEverything()
{
    ProgramStatusRecords records;
    records.apply(report("state=working:id=a"));
    records.apply(report("state=done"));
    records.apply(report("state=clear"));
    QVERIFY(records.isEmpty());
}

void ProgramStatusRecordsTest::testCapacityEvictsTheOldest()
{
    ProgramStatusRecords records(3);
    records.apply(report("state=working:id=one"));
    records.apply(report("state=working:id=two"));
    records.apply(report("state=working:id=three"));
    // Rewriting a record makes it recent again; it does not grow the set.
    records.apply(report("state=done:id=one"));
    records.apply(report("state=working:id=four"));
    QCOMPARE(records.size(), 3);
    QVERIFY(!records.record({QStringLiteral("two")}).has_value());
    QVERIFY(records.record({QStringLiteral("one")}).has_value());
}

void ProgramStatusRecordsTest::testMostUrgentWins()
{
    ProgramStatusRecords records;
    records.apply(report("state=blocked:id=a"));
    records.apply(report("state=working:id=b"));
    QCOMPARE(records.mostUrgent()->report.id, QStringList{QStringLiteral("a")});
    records.apply(report("state=error:id=c"));
    records.apply(report("state=blocked:id=d"));
    // Equally urgent: the most recently written wins.
    QCOMPARE(records.mostUrgent()->report.id, QStringList{QStringLiteral("d")});
}

void ProgramStatusRecordsTest::testAppIsInherited()
{
    ProgramStatusRecords records;
    records.apply(report("state=working:app=deploy"));
    records.apply(report("state=blocked:id=us/east"));
    QCOMPARE(records.mostUrgent()->report.app, QStringLiteral("deploy"));
}

void ProgramStatusRecordsTest::testMarkSeenKeepsText()
{
    ProgramStatusRecords records;
    records.apply(report("state=done:msg=RGVwbG95ZWQ"));
    QVERIFY(records.markSeen({ProgramStatusReport::Done, ProgramStatusReport::Error}));
    QCOMPARE(records.record({})->report.state, ProgramStatusReport::Idle);
    QCOMPARE(records.record({})->report.msg, QStringLiteral("Deployed"));
    QVERIFY(!records.markSeen({ProgramStatusReport::Done}));
}

void ProgramStatusRecordsTest::testReportSetsTheStatus()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report(QStringLiteral("state=working:app=claude-code:msg=%1").arg(QLatin1String(pushing)).toUtf8().constData()));
    QCOMPARE(text(status), QStringLiteral("working"));
    QCOMPARE(status.indicator().name(), QStringLiteral("#ff9500"));
    QCOMPARE(status.statusColor().name(), QStringLiteral("#ff9500"));
    QCOMPARE(detail(status), QStringLiteral("Pushing image"));

    controller.handle(report("state=blocked"));
    QCOMPARE(text(status), QStringLiteral("waiting"));
    QCOMPARE(status.indicator().name(), QStringLiteral("#5f87ff"));
    controller.handle(report("state=error"));
    QCOMPARE(status.indicator().name(), QStringLiteral("#ff3b30"));
    controller.handle(report("state=idle"));
    QCOMPARE(status.statusColor().name(), QStringLiteral("#888888"));
}

void ProgramStatusRecordsTest::testBlockedWithoutMessageDescribesTheKind()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=blocked:kind=permission"));
    QCOMPARE(detail(status), QStringLiteral("Waiting for approval"));
    controller.handle(report("state=blocked:kind=question"));
    QCOMPARE(detail(status), QStringLiteral("Waiting for an answer"));
    controller.handle(report("state=blocked:kind=auth"));
    QCOMPARE(detail(status), QStringLiteral("Waiting for login"));
}

void ProgramStatusRecordsTest::testChildTitlePrefixesTheDetail()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report(QStringLiteral("state=working:id=us-east:title=VVMgRWFzdA:msg=%1").arg(QLatin1String(pushing)).toUtf8().constData()));
    QCOMPARE(detail(status), QStringLiteral("US East: Pushing image"));
    // A child with only a title shows the title.
    controller.handle(report("state=error:id=us-east:title=VVMgRWFzdA"));
    QCOMPARE(detail(status), QStringLiteral("US East"));
    // The root record's title is the window title already, so it adds nothing.
    controller.handle(report("state=clear"));
    controller.handle(report("state=working:title=VVMgRWFzdA"));
    QCOMPARE(detail(status), QStringLiteral("<none>"));
}

void ProgramStatusRecordsTest::testReportWithoutMessageClearsTheDetail()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report(QStringLiteral("state=working:msg=%1").arg(QLatin1String(pushing)).toUtf8().constData()));
    controller.handle(report("state=working"));
    QVERIFY(!status.detail().has_value());
}

void ProgramStatusRecordsTest::testClearingTheLastRecordClearsTheStatus()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=working:id=a"));
    controller.handle(report("state=blocked:id=b"));
    controller.handle(report("state=clear:id=b"));
    QCOMPARE(text(status), QStringLiteral("working"));
    controller.handle(report("state=clear"));
    QVERIFY(!status.hasActiveStatus());
}

void ProgramStatusRecordsTest::testReportSupersedesAHookExpiration()
{
    // A hook said "working until the turn's progress ends". The program
    // reports idle itself when the user interrupts; that report wins and
    // nothing is left to expire.
    SessionStatus status(nullptr, 30);
    ProgramStatusController controller(&status);
    StatusUpdate working;
    working.fields.status = QStringLiteral("working");
    working.expiresOnProgressEnd = true;
    working.fallback.status = QStringLiteral("done");
    status.apply(working);
    controller.handle(report("state=idle"));
    QCOMPARE(text(status), QStringLiteral("idle"));
    QVERIFY(!status.hasArmedExpiration());
}

void ProgramStatusRecordsTest::testOtherWriterKeepsTheStatus()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=working"));

    // A hook writes after the record.
    StatusUpdate hook;
    hook.fields.status = QStringLiteral("waiting");
    hook.fields.detail = QStringLiteral("Allow Bash: rm -rf build?");
    status.apply(hook);

    // A prompt ends the working record, but the hook's status stays.
    controller.promptStarted();
    QCOMPARE(text(status), QStringLiteral("waiting"));
    QCOMPARE(detail(status), QStringLiteral("Allow Bash: rm -rf build?"));

    // Bookkeeping is not a write of the status: the next report still shows.
    StatusUpdate count;
    count.backgroundTasks = 1;
    status.apply(count);
    controller.handle(report("state=done"));
    QCOMPARE(text(status), QStringLiteral("done"));
}

void ProgramStatusRecordsTest::testPromptEndsTransientRecords()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=done:id=build"));
    controller.handle(report("state=blocked:id=deploy"));
    QCOMPARE(text(status), QStringLiteral("waiting"));
    controller.promptStarted();
    QCOMPARE(controller.records().size(), 1);
    QCOMPARE(text(status), QStringLiteral("done"));

    controller.handle(report("state=clear"));
    controller.handle(report("state=working"));
    controller.promptStarted();
    QVERIFY(!status.hasActiveStatus());
}

void ProgramStatusRecordsTest::testKeyPressMarksFinishedRecordsSeen()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=done:msg=RGVwbG95ZWQ"));
    controller.userDidPressKey();
    QCOMPARE(text(status), QStringLiteral("idle"));
    QCOMPARE(detail(status), QStringLiteral("Deployed"));

    // Working is not something to acknowledge.
    controller.handle(report("state=working"));
    controller.userDidPressKey();
    QCOMPARE(text(status), QStringLiteral("working"));
}

void ProgramStatusRecordsTest::testTerminalResetRemovesRecords()
{
    SessionStatus status;
    ProgramStatusController controller(&status);
    controller.handle(report("state=error"));
    controller.terminalReset();
    QVERIFY(controller.records().isEmpty());
    QVERIFY(!status.hasActiveStatus());
}

void ProgramStatusRecordsTest::testSanitizedText()
{
    // U+202E (right-to-left override) is built here rather than written, since
    // compilers rightly warn about it in source.
    const QString input = QStringLiteral("evil") + QChar(0x202E) + QStringLiteral("txt.exe line\u2028two \U0001F468\u200D\U0001F469");
    QCOMPARE(ProgramStatusController::sanitized(input), QStringLiteral("eviltxt.exe linetwo \U0001F468\u200D\U0001F469"));
}

QTEST_GUILESS_MAIN(ProgramStatusRecordsTest)

#include "ProgramStatusRecordsTest.moc"
