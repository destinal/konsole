// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDESESSIONSWIDGET_H
#define CLAUDESESSIONSWIDGET_H

#include <QWidget>

/**
 * The Claude Sessions panel: every Claude Code session in Konsole, with
 * its status. Empty for now.
 */
class ClaudeSessionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClaudeSessionsWidget(QWidget *parent = nullptr);
};

#endif // CLAUDESESSIONSWIDGET_H
