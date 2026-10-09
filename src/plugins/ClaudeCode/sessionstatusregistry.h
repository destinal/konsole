// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SESSIONSTATUSREGISTRY_H
#define SESSIONSTATUSREGISTRY_H

#include <QHash>
#include <QObject>

namespace Konsole
{
class Session;
}
class SessionStatus;

/**
 * Keeps a SessionStatus for every Konsole session, from the moment it is
 * created, and feeds it what the session's program reports: OSC 9;4
 * progress and OSC 21337 status.
 */
class SessionStatusRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SessionStatusRegistry(QObject *parent = nullptr);

    /** The status of @p session, or nullptr for a session that is not tracked. */
    SessionStatus *status(Konsole::Session *session) const;

    /**
     * The session named by @p identifier: its D-Bus path (/Sessions/3) or
     * its number (3). Returns nullptr when there is none.
     */
    Konsole::Session *session(const QString &identifier) const;

    QList<Konsole::Session *> sessions() const;

Q_SIGNALS:
    /** Something visible about @p session's status changed. */
    void statusChanged(Konsole::Session *session);

private:
    void track(Konsole::Session *session);

    QHash<Konsole::Session *, SessionStatus *> _statuses;
};

#endif // SESSIONSTATUSREGISTRY_H
