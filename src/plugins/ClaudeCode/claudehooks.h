// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDEHOOKS_H
#define CLAUDEHOOKS_H

#include <QObject>
#include <QString>

class QFileSystemWatcher;

/**
 * Whether Claude Code's settings run claude-code-hook for every event it
 * needs, and the means to change that, through konsole-claude-setup. The
 * settings are watched, so a change made elsewhere shows at once.
 *
 * Without the hooks, sessions still show what Claude Code reports itself
 * (OSC 7501), but not what the hooks add: detail lines and background work.
 */
class ClaudeHooks : public QObject
{
    Q_OBJECT

public:
    explicit ClaudeHooks(QObject *parent = nullptr);

    bool isInstalled() const;
    QString settingsPath() const;

    /** Runs konsole-claude-setup @p command; returns its output, or an error. */
    bool run(const QString &command, QString *output);

Q_SIGNALS:
    void installedChanged(bool installed);

private:
    void check();

    QFileSystemWatcher *_watcher;
    bool _installed = false;
};

#endif // CLAUDEHOOKS_H
