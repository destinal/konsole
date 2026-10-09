// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDECODEDBUS_H
#define CLAUDECODEDBUS_H

#include <QDBusContext>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class SessionStatus;
class SessionStatusRegistry;

/**
 * The plugin's D-Bus object, at /ClaudeCode on Konsole's own service,
 * interface org.kde.konsole.ClaudeCode. The it2 command that cc-status runs
 * reaches it through the service and session named in $TERM_SESSION_ID.
 *
 * Sessions are named by their D-Bus path (/Sessions/3) or number (3).
 * Errors: org.kde.konsole.ClaudeCode.NoSuchSession and
 * org.kde.konsole.ClaudeCode.InvalidArgument.
 */
class ClaudeCodeDBus : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.konsole.ClaudeCode")

public:
    ClaudeCodeDBus(SessionStatusRegistry *registry, QObject *parent);

    /** Registers the object on the session bus. Returns whether it succeeded. */
    bool registerObject();

public Q_SLOTS:
    /** Returns the plugin's version, to check that it is loaded. */
    QString ping() const;

    /** Returns the D-Bus paths of the sessions the plugin sees, such as /Sessions/1. */
    QStringList sessions() const;

    /**
     * Applies a status update, with the arguments iTerm2's set_session_status
     * takes (see StatusUpdate::fromArguments), and returns the status as it
     * then stands, as JSON. An update whose preconditions fail is dropped and
     * the status it was dropped against comes back.
     */
    QString setStatus(const QString &session, const QVariantMap &arguments);

    /** Returns the session's status as JSON (see SessionStatus::toJson). */
    QString getStatus(const QString &session);

    /** Returns the background-task count cc-status last parked for the session. */
    qlonglong getBackgroundTaskCount(const QString &session);

private:
    SessionStatus *statusOrError(const QString &session);

    SessionStatusRegistry *_registry;
};

#endif // CLAUDECODEDBUS_H
