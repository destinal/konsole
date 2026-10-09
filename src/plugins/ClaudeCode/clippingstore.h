// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLIPPINGSTORE_H
#define CLIPPINGSTORE_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace Konsole
{
class Session;
}

/** One finding a code review filed with `it2 session add-clipping`. */
struct Clipping {
    QString type;
    QString title;
    QString detail;
    QDateTime added;

    /** The clipping as it is sent to Chat: the title in bold, then the detail. */
    QString formatted() const;
};

/**
 * The clippings filed with each session: live ones, waiting to be acted on,
 * and archived ones, kept as history. A workgroup's clippings belong to its
 * Chat session. Kept in memory only.
 */
class ClippingStore : public QObject
{
    Q_OBJECT

public:
    explicit ClippingStore(QObject *parent = nullptr);

    QList<Clipping> live(Konsole::Session *session) const;
    QList<Clipping> archived(Konsole::Session *session) const;

    void add(Konsole::Session *session, const Clipping &clipping);
    /** Moves every live clipping to the archive. */
    void archive(Konsole::Session *session);
    /** Moves the live clippings at @p indexes to the archive. */
    void archive(Konsole::Session *session, QList<int> indexes);

    /** @p clippings formatted for Chat, separated by "--" lines. */
    static QString formatted(const QList<Clipping> &clippings);

Q_SIGNALS:
    void changed(Konsole::Session *session);
    /** A clipping was added to @p session. */
    void added(Konsole::Session *session);

private:
    struct Clippings {
        QList<Clipping> live;
        QList<Clipping> archived;
    };
    void watch(Konsole::Session *session);

    QHash<Konsole::Session *, Clippings> _clippings;
};

#endif // CLIPPINGSTORE_H
