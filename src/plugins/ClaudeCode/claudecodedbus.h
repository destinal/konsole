// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDECODEDBUS_H
#define CLAUDECODEDBUS_H

#include <QObject>
#include <QStringList>

/**
 * The plugin's D-Bus object, at /ClaudeCode on Konsole's own service.
 * Hooks running in a Konsole session reach it through
 * $KONSOLE_DBUS_SERVICE.
 */
class ClaudeCodeDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.konsole.ClaudeCode")

public:
    explicit ClaudeCodeDBus(QObject *parent);

    /** Registers the object on the session bus. Returns whether it succeeded. */
    bool registerObject();

public Q_SLOTS:
    /** Returns the plugin's version, to check that it is loaded. */
    QString ping() const;

    /** Returns the D-Bus paths of the sessions the plugin sees, such as /Sessions/1. */
    QStringList sessions() const;
};

#endif // CLAUDECODEDBUS_H
