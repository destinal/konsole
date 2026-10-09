// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef STATUSNOTIFIER_H
#define STATUSNOTIFIER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>

#include <optional>

namespace Konsole
{
class Session;
}
class SessionStatusRegistry;

/**
 * Sends a desktop notification when a session the user is not looking at
 * starts waiting for them, or finishes or fails: the ClaudeCodeWaiting and
 * ClaudeCodeFinished events, configurable in Konsole's notification
 * settings. Clicking it brings the session forward.
 */
class StatusNotifier : public QObject
{
    Q_OBJECT

public:
    explicit StatusNotifier(SessionStatusRegistry *registry, QObject *parent = nullptr);

private:
    void statusChanged(Konsole::Session *session, const std::optional<QString> &previousStatusText);

    struct Sent {
        QString event;
        QElapsedTimer timer;
    };

    SessionStatusRegistry *_registry;
    // The last notification for each session, to rate-limit repeats.
    QHash<Konsole::Session *, Sent> _sent;
};

#endif // STATUSNOTIFIER_H
