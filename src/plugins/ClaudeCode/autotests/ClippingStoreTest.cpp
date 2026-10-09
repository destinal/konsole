// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clippingstore.h"

#include "session/Session.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
Clipping clipping(const char *title, const char *detail)
{
    return Clipping{QStringLiteral("Code Review Comment"), QString::fromUtf8(title), QString::fromUtf8(detail), QDateTime()};
}

QStringList titles(const QList<Clipping> &clippings)
{
    QStringList result;
    for (const Clipping &c : clippings) {
        result.append(c.title);
    }
    return result;
}
}

class ClippingStoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFormatted();
    void testAddAndArchive();
    void testArchiveSome();
    void testSessionsAreSeparateAndForgotten();
};

void ClippingStoreTest::testFormatted()
{
    QCOMPARE(clipping("Leak", "In f()").formatted(), QStringLiteral("**Leak**\nIn f()"));
    // No title: just the detail.
    QCOMPARE(clipping(" ", "In f()").formatted(), QStringLiteral("In f()"));
    QCOMPARE(ClippingStore::formatted({clipping("A", "a"), clipping("B", "b")}), QStringLiteral("**A**\na\n--\n**B**\nb"));
}

void ClippingStoreTest::testAddAndArchive()
{
    Konsole::Session session;
    ClippingStore store;
    QSignalSpy changed(&store, &ClippingStore::changed);
    QSignalSpy added(&store, &ClippingStore::added);

    store.add(&session, clipping("A", "a"));
    store.add(&session, clipping("B", "b"));
    QCOMPARE(titles(store.live(&session)), (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
    QCOMPARE(added.count(), 2);

    store.archive(&session);
    QVERIFY(store.live(&session).isEmpty());
    QCOMPARE(titles(store.archived(&session)), (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
    QCOMPARE(changed.count(), 3);

    // Nothing live: no change.
    store.archive(&session);
    QCOMPARE(changed.count(), 3);
}

void ClippingStoreTest::testArchiveSome()
{
    Konsole::Session session;
    ClippingStore store;
    for (const char *title : {"A", "B", "C", "D"}) {
        store.add(&session, clipping(title, ""));
    }
    // Unordered, repeated and out-of-range indexes are fine.
    store.archive(&session, {2, 0, 2, 9});
    QCOMPARE(titles(store.live(&session)), (QStringList{QStringLiteral("B"), QStringLiteral("D")}));
    QCOMPARE(store.archived(&session).size(), 2);
}

void ClippingStoreTest::testSessionsAreSeparateAndForgotten()
{
    ClippingStore store;
    auto *first = new Konsole::Session();
    Konsole::Session second;
    store.add(first, clipping("A", ""));
    store.add(&second, clipping("B", ""));
    QCOMPARE(titles(store.live(&second)), QStringList{QStringLiteral("B")});

    delete first;
    QVERIFY(store.live(first).isEmpty());
    QCOMPARE(store.live(&second).size(), 1);
}

QTEST_MAIN(ClippingStoreTest)

#include "ClippingStoreTest.moc"
