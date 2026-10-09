// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-FileCopyrightText: George Nachman and the iTerm2 authors
// SPDX-License-Identifier: GPL-2.0-only
//
// Ported from iTerm2's SetStatusBuiltInFunctionTests, SessionTabStatusBackgroundTasksTests and TabStatusControllerTests.

// Ports the status-model tests of iTerm2 (SetStatusBuiltInFunctionTests,
// SessionTabStatusBackgroundTasksTests, TabStatusControllerTests), so the
// plugin behaves the way cc-status expects iTerm2 to.

#include "sessionstatus.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

#include <limits>

using Konsole::TabStatusFields;
using Konsole::TerminalProgress;

namespace
{
// Short enough to keep the tests fast, long enough to land updates inside.
constexpr int delay = 30;

const std::optional<TerminalProgress> indeterminate = TerminalProgress{TerminalProgress::Indeterminate, 0};
const std::optional<TerminalProgress> stopped = std::nullopt;

StatusUpdate working(bool expiring)
{
    StatusUpdate update;
    update.fields.status = QStringLiteral("working");
    if (expiring) {
        update.expiresOnProgressEnd = true;
        update.fallback.status = QStringLiteral("idle");
    }
    return update;
}

StatusUpdate withStatus(const QString &status)
{
    StatusUpdate update;
    update.fields.status = status;
    return update;
}

StatusUpdate bookkeeping(qint64 count)
{
    StatusUpdate update;
    update.backgroundTasks = count;
    return update;
}

StatusUpdate delta(qint64 value)
{
    StatusUpdate update;
    update.backgroundTasksDelta = value;
    return update;
}

StatusUpdate turnOpen(bool open)
{
    StatusUpdate update;
    update.turnOpen = open;
    return update;
}

void endOperation(SessionStatus &status)
{
    status.progressReported(indeterminate);
    status.progressReported(stopped);
}

void settle()
{
    QTest::qWait(delay * 4);
}

std::optional<StatusUpdate> parse(const QVariantMap &arguments, QString *error = nullptr)
{
    QString ignored;
    return StatusUpdate::fromArguments(arguments, error != nullptr ? error : &ignored);
}

QString text(const SessionStatus &status)
{
    return status.statusText().value_or(QStringLiteral("<none>"));
}
}

class SessionStatusTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Arguments, as it2 set-status passes them.
    void testTurnOpenArgument();
    void testDeltaArgument();
    void testCountAndDeltaTogetherAreRefused();
    void testNegativeCountIsClamped();
    void testPreconditionArguments();
    void testInvalidColorsAreRefused();
    void testEmptyArgumentsClearAndMissingOnesLeaveAlone();
    void testExpiresOnArguments();

    // The JSON answer.
    void testJsonBeforeAnyStatus();
    void testJsonCarriesEveryField();

    // Background-task count and turn flag.
    void testCountIsBookkeeping();
    void testDeltaAddsAndClamps();
    void testDeltaSaturates();
    void testTurnFlag();

    // Preconditions.
    void testPreconditionsDropTheWholeUpdate();
    void testUnknownTurnFailsEitherPrecondition();
    void testPreconditionsThatHoldApply();
    void testDroppedUpdateLeavesTheExpirationArmed();

    // Expiring at the end of an operation.
    void testOperationEndExpiresTheStatus();
    void testProgressOnlySessionHasNoStatus();
    void testStopWithoutStartDoesNothing();
    void testAssertionDuringTheHeadStartWins();
    void testProgressResumingDuringTheHeadStartCancels();
    void testErrorEndsTheOperation();
    void testPausedDoesNotEndTheOperation();
    void testAssertionWithoutExpirationDisarms();
    void testLaterAssertionRearmsWithItsOwnFallback();
    void testArmingWithoutVisibleChange();
    void testBookkeepingDoesNotDisarm();
    void testExpirationIsNotReused();
    void testFallbackWithNoFieldsClears();

    // Background work holding an expiration.
    void testBackgroundWorkHoldsThenReleases();
    void testCountInsideTheHeadStartDoesNotCutItShort();
    void testReassertingWhileHeldKeepsTheCountAbleToRelease();
    void testReassertingWhileHeldWithoutExpirationDisarms();
    void testDeltaReachingZeroReleases();
    void testBookkeepingDoesNotReleaseANewTurn();

    // Lifecycle and key presses.
    void testClearSupersedesAPendingExpiration();
    void testProgramExitDropsEverything();
    void testAfterExitAStopCannotEndAnUnseenOperation();
    void testChangesCarryThePreviousText();
    void testKeyPressTurnsDoneAndErrorIdle();
    void testKeyPressLeavesOtherStatusesAlone();

    // OSC 21337.
    void testTabStatusUpdateConverts();
};

void SessionStatusTest::testTurnOpenArgument()
{
    QCOMPARE(parse({{QStringLiteral("turn_open"), true}})->turnOpen, std::optional<bool>(true));
    QCOMPARE(parse({{QStringLiteral("turn_open"), false}})->turnOpen, std::optional<bool>(false));
    const auto update = parse({{QStringLiteral("status"), QStringLiteral("working")}});
    QVERIFY(!update->turnOpen.has_value());
    QCOMPARE(update->fields.status, QStringLiteral("working"));
}

void SessionStatusTest::testDeltaArgument()
{
    const auto update = parse({{QStringLiteral("background_tasks_delta"), -1}});
    QVERIFY(!update->backgroundTasks.has_value());
    QCOMPARE(update->backgroundTasksDelta, std::optional<qint64>(-1));
}

void SessionStatusTest::testCountAndDeltaTogetherAreRefused()
{
    QString error;
    QVERIFY(!parse({{QStringLiteral("background_tasks"), 2}, {QStringLiteral("background_tasks_delta"), 1}}, &error).has_value());
    QCOMPARE(error, QStringLiteral("Pass background_tasks or background_tasks_delta, not both."));
}

void SessionStatusTest::testNegativeCountIsClamped()
{
    QCOMPARE(parse({{QStringLiteral("background_tasks"), -3}})->backgroundTasks, std::optional<qint64>(0));
}

void SessionStatusTest::testPreconditionArguments()
{
    const auto update =
        parse({{QStringLiteral("status"), QStringLiteral("idle")}, {QStringLiteral("if_turn_open"), false}, {QStringLiteral("if_background_tasks"), 0}});
    QCOMPARE(update->requiredTurnOpen, std::optional<bool>(false));
    QCOMPARE(update->requiredBackgroundTasks, std::optional<qint64>(0));

    const auto none = parse({{QStringLiteral("status"), QStringLiteral("idle")}});
    QVERIFY(!none->requiredTurnOpen.has_value());
    QVERIFY(!none->requiredBackgroundTasks.has_value());
}

void SessionStatusTest::testInvalidColorsAreRefused()
{
    QString error;
    QVERIFY(!parse({{QStringLiteral("dot_color"), QStringLiteral("orange")}}, &error).has_value());
    QCOMPARE(error, QStringLiteral("Invalid dot_color (expected #rrggbb)"));
    QVERIFY(!parse({{QStringLiteral("text_color"), QStringLiteral("#12")}}, &error).has_value());
    QCOMPARE(error, QStringLiteral("Invalid text_color (expected #rrggbb)"));
    QVERIFY(!parse({{QStringLiteral("expires_on"), QStringLiteral("progress-end")}, {QStringLiteral("then_dot_color"), QStringLiteral("nope")}}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("Invalid then_dot_color (expected #rrggbb)"));

    const auto update = parse({{QStringLiteral("dot_color"), QStringLiteral("#ff9500")}, {QStringLiteral("text_color"), QStringLiteral("#888")}});
    QCOMPARE(update->fields.indicator->name(), QStringLiteral("#ff9500"));
    QCOMPARE(update->fields.statusColor->name(), QStringLiteral("#888888"));
}

void SessionStatusTest::testEmptyArgumentsClearAndMissingOnesLeaveAlone()
{
    const auto update = parse({{QStringLiteral("status"), QString()},
                               {QStringLiteral("dot_color"), QString()},
                               {QStringLiteral("text_color"), QString()},
                               {QStringLiteral("detail"), QString()}});
    QCOMPARE(update->fields, TabStatusFields::cleared());
    QCOMPARE(parse({})->fields, TabStatusFields());
}

void SessionStatusTest::testExpiresOnArguments()
{
    // Without then_ arguments the status is cleared when it expires.
    auto update = parse({{QStringLiteral("status"), QStringLiteral("working")}, {QStringLiteral("expires_on"), QStringLiteral("progress-end")}});
    QVERIFY(update->expiresOnProgressEnd);
    QCOMPARE(update->fallback, TabStatusFields::cleared());

    update = parse({{QStringLiteral("expires_on"), QStringLiteral("progress-end")},
                    {QStringLiteral("then_status"), QStringLiteral("idle")},
                    {QStringLiteral("then_dot_color"), QStringLiteral("#00d75f")},
                    {QStringLiteral("then_detail"), QString()}});
    QCOMPARE(update->fallback.status, QStringLiteral("idle"));
    QCOMPARE(update->fallback.indicator->name(), QStringLiteral("#00d75f"));
    QCOMPARE(update->fallback.detail, QString());
    QVERIFY(!update->fallback.statusColor.has_value());

    // An empty expires_on means no expiration.
    QVERIFY(!parse({{QStringLiteral("expires_on"), QString()}})->expiresOnProgressEnd);

    QString error;
    QVERIFY(!parse({{QStringLiteral("expires_on"), QStringLiteral("tomorrow")}}, &error).has_value());
    QVERIFY(error.startsWith(QStringLiteral("Unknown expires_on")));
    QVERIFY(!parse({{QStringLiteral("then_status"), QStringLiteral("idle")}}, &error).has_value());
    QCOMPARE(error, QStringLiteral("The then_ arguments only mean something with expires_on, which was not given."));
}

void SessionStatusTest::testJsonBeforeAnyStatus()
{
    SessionStatus status;
    const QJsonObject json = status.toJson();
    QCOMPARE(json.keys(),
             (QStringList{QStringLiteral("background_tasks"),
                          QStringLiteral("detail"),
                          QStringLiteral("dot_color"),
                          QStringLiteral("status"),
                          QStringLiteral("text_color"),
                          QStringLiteral("turn_open")}));
    QCOMPARE(json.value(QStringLiteral("background_tasks")).toInteger(), 0);
    for (const char *key : {"status", "text_color", "dot_color", "detail", "turn_open"}) {
        QVERIFY2(json.value(QLatin1String(key)).isNull(), key);
    }
}

void SessionStatusTest::testJsonCarriesEveryField()
{
    SessionStatus status;
    StatusUpdate update = withStatus(QStringLiteral("working"));
    update.fields.detail = QStringLiteral("1 background task running");
    update.fields.indicator = QColor(QStringLiteral("#ff9500"));
    update.backgroundTasks = 1;
    update.turnOpen = false;
    status.apply(update);

    const QString json = QString::fromUtf8(QJsonDocument(status.toJson()).toJson(QJsonDocument::Compact));
    QVERIFY(json.contains(QStringLiteral("\"turn_open\":false")));
    QVERIFY(json.contains(QStringLiteral("\"status\":\"working\"")));
    QVERIFY(json.contains(QStringLiteral("\"detail\":\"1 background task running\"")));
    QVERIFY(json.contains(QStringLiteral("\"background_tasks\":1")));
    QVERIFY(json.contains(QStringLiteral("\"dot_color\":\"#ff9500\"")));
    QVERIFY(json.contains(QStringLiteral("\"text_color\":null")));
}

void SessionStatusTest::testCountIsBookkeeping()
{
    SessionStatus status;
    QSignalSpy changed(&status, &SessionStatus::changed);
    status.apply(bookkeeping(3));
    QCOMPARE(status.backgroundTasks(), 3);
    QCOMPARE(changed.count(), 0);

    StatusUpdate visible = bookkeeping(3);
    visible.fields.status = QStringLiteral("working");
    status.apply(visible);
    QCOMPARE(changed.count(), 1);

    // An update without a count leaves it alone.
    status.apply(withStatus(QStringLiteral("idle")));
    QCOMPARE(status.backgroundTasks(), 3);
}

void SessionStatusTest::testDeltaAddsAndClamps()
{
    SessionStatus status;
    status.apply(delta(1));
    status.apply(delta(1));
    status.apply(delta(-1));
    QCOMPARE(status.backgroundTasks(), 1);
    status.apply(delta(-5));
    QCOMPARE(status.backgroundTasks(), 0);
}

void SessionStatusTest::testDeltaSaturates()
{
    SessionStatus status;
    status.apply(bookkeeping(1));
    status.apply(delta(std::numeric_limits<qint64>::max()));
    QCOMPARE(status.backgroundTasks(), std::numeric_limits<qint64>::max());
    status.apply(delta(std::numeric_limits<qint64>::min()));
    QCOMPARE(status.backgroundTasks(), 0);
    status.apply(delta(std::numeric_limits<qint64>::min()));
    QCOMPARE(status.backgroundTasks(), 0);
}

void SessionStatusTest::testTurnFlag()
{
    SessionStatus status;
    QSignalSpy changed(&status, &SessionStatus::changed);
    QVERIFY(!status.turnOpen().has_value());
    status.apply(turnOpen(true));
    QCOMPARE(status.turnOpen(), std::optional<bool>(true));
    // Unrelated updates leave it alone; it never counts as a visible change.
    status.apply(delta(1));
    QCOMPARE(status.turnOpen(), std::optional<bool>(true));
    status.apply(turnOpen(false));
    QCOMPARE(status.turnOpen(), std::optional<bool>(false));
    QCOMPARE(changed.count(), 0);
}

void SessionStatusTest::testPreconditionsDropTheWholeUpdate()
{
    SessionStatus status;
    StatusUpdate start = working(false);
    start.turnOpen = true;
    status.apply(start);

    StatusUpdate idle = withStatus(QStringLiteral("idle"));
    idle.fields.detail = QStringLiteral("DONE");
    idle.requiredTurnOpen = false;
    QVERIFY(!status.apply(idle));
    QCOMPARE(text(status), QStringLiteral("working"));
    QVERIFY(!status.detail().has_value());

    // The bookkeeping in a dropped update is dropped too.
    StatusUpdate bookkept = delta(1);
    bookkept.turnOpen = false;
    bookkept.requiredTurnOpen = false;
    QVERIFY(!status.apply(bookkept));
    QCOMPARE(status.backgroundTasks(), 0);
    QCOMPARE(status.turnOpen(), std::optional<bool>(true));

    SessionStatus counted;
    counted.apply(bookkeeping(1));
    StatusUpdate whenIdle = withStatus(QStringLiteral("idle"));
    whenIdle.requiredBackgroundTasks = 0;
    QVERIFY(!counted.apply(whenIdle));
    QVERIFY(!counted.statusText().has_value());
}

void SessionStatusTest::testUnknownTurnFailsEitherPrecondition()
{
    SessionStatus status;
    StatusUpdate update = withStatus(QStringLiteral("idle"));
    update.requiredTurnOpen = false;
    QVERIFY(!status.apply(update));
    update.requiredTurnOpen = true;
    QVERIFY(!status.apply(update));
    QVERIFY(!status.statusText().has_value());
}

void SessionStatusTest::testPreconditionsThatHoldApply()
{
    SessionStatus status;
    StatusUpdate start = working(false);
    start.turnOpen = false;
    status.apply(start);
    StatusUpdate idle = withStatus(QStringLiteral("idle"));
    idle.requiredTurnOpen = false;
    idle.requiredBackgroundTasks = 0;
    QVERIFY(status.apply(idle));
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testDroppedUpdateLeavesTheExpirationArmed()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    StatusUpdate idle = withStatus(QStringLiteral("idle"));
    idle.requiredTurnOpen = false;
    QVERIFY(!status.apply(idle));
    QVERIFY(status.hasArmedExpiration());
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testOperationEndExpiresTheStatus()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    // Nothing happens before the head start elapses.
    QCOMPARE(text(status), QStringLiteral("working"));
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testProgressOnlySessionHasNoStatus()
{
    SessionStatus status(nullptr, delay);
    QSignalSpy changed(&status, &SessionStatus::changed);
    endOperation(status);
    settle();
    QVERIFY(!status.hasActiveStatus());
    QVERIFY(!status.hasArmedExpiration());
    QCOMPARE(changed.count(), 0);
}

void SessionStatusTest::testStopWithoutStartDoesNothing()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    status.progressReported(stopped);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
    QVERIFY(status.hasArmedExpiration());
}

void SessionStatusTest::testAssertionDuringTheHeadStartWins()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    status.apply(withStatus(QStringLiteral("waiting")));
    settle();
    QCOMPARE(text(status), QStringLiteral("waiting"));
    QVERIFY(!status.hasArmedExpiration());
}

void SessionStatusTest::testProgressResumingDuringTheHeadStartCancels()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    status.progressReported(indeterminate);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
    QVERIFY(status.hasArmedExpiration());
    // The expiration waits for the next end.
    status.progressReported(stopped);
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testErrorEndsTheOperation()
{
    for (int percent : {0, 50}) {
        SessionStatus status(nullptr, delay);
        status.apply(working(true));
        status.progressReported(indeterminate);
        status.progressReported(TerminalProgress{TerminalProgress::Error, percent});
        settle();
        QCOMPARE(text(status), QStringLiteral("idle"));
    }
}

void SessionStatusTest::testPausedDoesNotEndTheOperation()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    status.progressReported(indeterminate);
    status.progressReported(TerminalProgress{TerminalProgress::Paused, 50});
    status.progressReported(TerminalProgress{TerminalProgress::Normal, 70});
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
}

void SessionStatusTest::testAssertionWithoutExpirationDisarms()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    QVERIFY(status.hasArmedExpiration());
    status.apply(working(false));
    QVERIFY(!status.hasArmedExpiration());
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
}

void SessionStatusTest::testLaterAssertionRearmsWithItsOwnFallback()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    StatusUpdate waiting = withStatus(QStringLiteral("waiting"));
    waiting.expiresOnProgressEnd = true;
    waiting.fallback.status = QStringLiteral("done");
    status.apply(waiting);
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("done"));
}

void SessionStatusTest::testArmingWithoutVisibleChange()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(false));
    QSignalSpy changed(&status, &SessionStatus::changed);
    status.apply(working(true));
    QCOMPARE(changed.count(), 0);
    QVERIFY(status.hasArmedExpiration());
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));

    // An expiration needs no visible field at all.
    SessionStatus bare(nullptr, delay);
    StatusUpdate update;
    update.expiresOnProgressEnd = true;
    update.fallback = TabStatusFields::cleared();
    bare.apply(update);
    QVERIFY(!bare.hasActiveStatus());
    QVERIFY(bare.hasArmedExpiration());
}

void SessionStatusTest::testBookkeepingDoesNotDisarm()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    status.apply(bookkeeping(3));
    QVERIFY(status.hasArmedExpiration());
    QCOMPARE(status.backgroundTasks(), 3);
    QVERIFY(!bookkeeping(1).assertsStatus());
    QVERIFY(working(false).assertsStatus());
}

void SessionStatusTest::testExpirationIsNotReused()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));
    QVERIFY(!status.hasArmedExpiration());
    status.apply(withStatus(QStringLiteral("busy")));
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("busy"));
}

void SessionStatusTest::testFallbackWithNoFieldsClears()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(false);
    update.fields.indicator = QColor(Qt::red);
    update.expiresOnProgressEnd = true;
    update.fallback = TabStatusFields::cleared();
    status.apply(update);
    endOperation(status);
    settle();
    QVERIFY(!status.statusText().has_value());
    QVERIFY(!status.hasActiveStatus());
}

void SessionStatusTest::testBackgroundWorkHoldsThenReleases()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(true);
    update.backgroundTasks = 2;
    status.apply(update);
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
    // The count reaching zero releases it at once.
    status.apply(bookkeeping(0));
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testCountInsideTheHeadStartDoesNotCutItShort()
{
    SessionStatus status(nullptr, 3600 * 1000);
    StatusUpdate update = working(true);
    update.backgroundTasks = 2;
    status.apply(update);
    endOperation(status);
    status.apply(bookkeeping(0));
    QCOMPARE(text(status), QStringLiteral("working"));
    QVERIFY(status.hasArmedExpiration());
}

void SessionStatusTest::testReassertingWhileHeldKeepsTheCountAbleToRelease()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(true);
    update.backgroundTasks = 2;
    status.apply(update);
    endOperation(status);
    settle();
    update.backgroundTasks = 1;
    status.apply(update);
    QCOMPARE(text(status), QStringLiteral("working"));
    status.apply(bookkeeping(0));
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testReassertingWhileHeldWithoutExpirationDisarms()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(true);
    update.backgroundTasks = 2;
    status.apply(update);
    endOperation(status);
    settle();
    StatusUpdate plain = working(false);
    plain.backgroundTasks = 1;
    status.apply(plain);
    status.apply(bookkeeping(0));
    QCOMPARE(text(status), QStringLiteral("working"));
    QVERIFY(!status.hasArmedExpiration());
}

void SessionStatusTest::testDeltaReachingZeroReleases()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(true);
    update.backgroundTasks = 1;
    status.apply(update);
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
    status.apply(delta(-1));
    QCOMPARE(text(status), QStringLiteral("idle"));
}

void SessionStatusTest::testBookkeepingDoesNotReleaseANewTurn()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    settle();
    QCOMPARE(text(status), QStringLiteral("idle"));
    // A new turn armed before its operation starts must wait for its own end.
    status.apply(working(true));
    status.apply(bookkeeping(0));
    QCOMPARE(text(status), QStringLiteral("working"));
}

void SessionStatusTest::testClearSupersedesAPendingExpiration()
{
    SessionStatus status(nullptr, delay);
    StatusUpdate update = working(true);
    update.backgroundTasks = 2;
    update.turnOpen = true;
    status.apply(update);
    endOperation(status);
    QVERIFY(status.clearStatus());
    QVERIFY(!status.hasArmedExpiration());
    settle();
    QVERIFY(!status.statusText().has_value());
    QCOMPARE(status.backgroundTasks(), 0);
    QVERIFY(!status.turnOpen().has_value());
    // Nothing left to clear.
    QVERIFY(!status.clearStatus());
}

void SessionStatusTest::testProgramExitDropsEverything()
{
    SessionStatus status(nullptr, delay);
    status.apply(working(true));
    endOperation(status);
    status.programDidExit();
    QSignalSpy changed(&status, &SessionStatus::changed);
    settle();
    QVERIFY(!status.hasActiveStatus());
    QVERIFY(!status.hasArmedExpiration());
    // The stale timer brings nothing back.
    QCOMPARE(changed.count(), 0);
}

void SessionStatusTest::testAfterExitAStopCannotEndAnUnseenOperation()
{
    SessionStatus status(nullptr, delay);
    status.progressReported(indeterminate);
    status.programDidExit();
    status.apply(working(true));
    status.progressReported(stopped);
    settle();
    QCOMPARE(text(status), QStringLiteral("working"));
}

void SessionStatusTest::testChangesCarryThePreviousText()
{
    SessionStatus status(nullptr, delay);
    QSignalSpy changed(&status, &SessionStatus::changed);
    status.apply(working(true));
    endOperation(status);
    settle();
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(0).at(0).value<std::optional<QString>>(), std::nullopt);
    QCOMPARE(changed.at(1).at(0).value<std::optional<QString>>(), QStringLiteral("working"));
}

void SessionStatusTest::testKeyPressTurnsDoneAndErrorIdle()
{
    for (const QString &finished : {QStringLiteral("done"), QStringLiteral("Error")}) {
        SessionStatus status;
        StatusUpdate update = withStatus(finished);
        update.fields.detail = QStringLiteral("All tests pass");
        status.apply(update);
        status.userDidPressKey();
        QCOMPARE(text(status), QStringLiteral("idle"));
        QCOMPARE(status.detail(), QStringLiteral("All tests pass"));
        QVERIFY(status.indicator().isValid());
    }
}

void SessionStatusTest::testKeyPressLeavesOtherStatusesAlone()
{
    for (const QString &other : {QStringLiteral("working"), QStringLiteral("waiting"), QStringLiteral("idle"), QStringLiteral("Deploying")}) {
        SessionStatus status;
        status.apply(withStatus(other));
        QSignalSpy changed(&status, &SessionStatus::changed);
        status.userDidPressKey();
        QCOMPARE(text(status), other);
        QCOMPARE(changed.count(), 0);
    }
    // No status: a key press creates none.
    SessionStatus empty;
    empty.userDidPressKey();
    QVERIFY(!empty.hasActiveStatus());
}

void SessionStatusTest::testTabStatusUpdateConverts()
{
    SessionStatus status(nullptr, delay);
    status.apply(StatusUpdate::fromTabStatus(Konsole::TabStatusUpdate::fromOsc21337(u"status=Working;indicator=#ffa500;expires-on=progress-end")));
    QCOMPARE(text(status), QStringLiteral("Working"));
    QCOMPARE(status.indicator().name(), QStringLiteral("#ffa500"));
    endOperation(status);
    settle();
    // No then- keys: the status is cleared.
    QVERIFY(!status.hasActiveStatus());
}

QTEST_GUILESS_MAIN(SessionStatusTest)

#include "SessionStatusTest.moc"
