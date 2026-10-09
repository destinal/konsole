/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SessionTest.h"

#include <QSignalSpy>
#include <QTest>

// Konsole
#include "../Emulation.h"
#include "../session/Session.h"

using namespace Konsole;

void SessionTest::testNoProfile()
{
    auto session = new Session();

    // No profile loaded, nothing to run
    QCOMPARE(session->isRunning(), false);
    QCOMPARE(session->sessionId(), 1);
    QCOMPARE(session->isRemote(), false);
    QCOMPARE(session->program(), QString());
    QCOMPARE(session->arguments(), QStringList());
    QCOMPARE(session->tabTitleFormat(Session::LocalTabTitle), QString());
    QCOMPARE(session->tabTitleFormat(Session::RemoteTabTitle), QString());

    delete session;
}

void SessionTest::testEmulation()
{
    auto session = new Session();

    Emulation *emulation = session->emulation();

    QCOMPARE(emulation->lineCount(), 40);

    delete session;
}

void SessionTest::testUserVars()
{
    auto session = new Session();
    QSignalSpy changed(session, &Session::userVarChanged);

    // The escape sequence reaches the session through the emulation.
    const char sequence[] = "\033]1337;SetUserVar=cc_status=d29ya2luZw==\007";
    session->emulation()->receiveData(sequence, sizeof(sequence) - 1);
    QCOMPARE(session->userVar(QStringLiteral("cc_status")), QStringLiteral("working"));
    QCOMPARE(changed.count(), 1);

    // Setting the same value again does not signal.
    session->setUserVar(QStringLiteral("cc_status"), QStringLiteral("working"));
    QCOMPARE(changed.count(), 1);

    session->setUserVar(QStringLiteral("cc_status"), QStringLiteral("idle"));
    QCOMPARE(session->userVar(QStringLiteral("cc_status")), QStringLiteral("idle"));
    QCOMPARE(changed.count(), 2);

    // Invalid names and oversized values are refused.
    session->setUserVar(QStringLiteral("bad name"), QStringLiteral("x"));
    session->setUserVar(QStringLiteral("big"), QString(64 * 1024 + 1, QLatin1Char('x')));
    QCOMPARE(session->userVarNames(), QStringList{QStringLiteral("cc_status")});

    // At most 256 variables are held.
    for (int i = 0; i < 300; i++) {
        session->setUserVar(QStringLiteral("v%1").arg(i), QStringLiteral("x"));
    }
    QCOMPARE(session->userVarNames().size(), 256);
    QCOMPARE(session->userVar(QStringLiteral("cc_status")), QStringLiteral("idle"));

    delete session;
}

void SessionTest::testTabStatus()
{
    auto session = new Session();
    QSignalSpy received(session, &Session::tabStatusReceived);

    const char sequence[] = "\033]21337;status=Working;expires-on=progress-end;then-status=idle\007";
    session->emulation()->receiveData(sequence, sizeof(sequence) - 1);

    QCOMPARE(received.count(), 1);
    const auto update = received.at(0).at(0).value<TabStatusUpdate>();
    QCOMPARE(update.fields.status, QStringLiteral("Working"));
    QVERIFY(update.expiresOnProgressEnd);
    QCOMPARE(update.fallback.status, QStringLiteral("idle"));

    delete session;
}

QTEST_MAIN(SessionTest)

#include "moc_SessionTest.cpp"
