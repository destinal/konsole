// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef STATUSBADGES_H
#define STATUSBADGES_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

namespace Konsole
{
class Session;
}
class SessionStatusRegistry;

/**
 * Optionally shows each session's status as its badge, the label Konsole
 * draws in the terminal's top-right corner. Off by default; the setting is
 * kept in konsolerc. While a status badge shows, the session's own badge
 * settings are put aside and come back as they were when the status goes or
 * badges are turned off.
 */
class StatusBadges : public QObject
{
    Q_OBJECT

public:
    explicit StatusBadges(SessionStatusRegistry *registry, QObject *parent = nullptr);

    bool isEnabled() const;
    void setEnabled(bool enabled);

Q_SIGNALS:
    void enabledChanged(bool enabled);

private:
    struct Badge {
        bool enabled = false;
        QString text;
        QColor color;
        bool textOnly = false;
    };

    void update(Konsole::Session *session);
    static Badge badgeOf(Konsole::Session *session);
    static void setBadge(Konsole::Session *session, const Badge &badge);

    SessionStatusRegistry *_registry;
    bool _enabled;
    // The badge each session had before a status badge replaced it.
    QHash<Konsole::Session *, Badge> _saved;
    QSet<Konsole::Session *> _watched;
};

#endif // STATUSBADGES_H
