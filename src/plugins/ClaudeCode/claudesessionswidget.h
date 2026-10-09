// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDESESSIONSWIDGET_H
#define CLAUDESESSIONSWIDGET_H

#include <QWidget>

class ClaudeSessionsModel;
class QListView;
class QStackedLayout;

/**
 * The Claude Sessions panel: every session that shows a status, in every
 * Konsole window, the ones needing the user first. Clicking a row brings
 * that session forward.
 */
class ClaudeSessionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClaudeSessionsWidget(ClaudeSessionsModel *model, QWidget *parent = nullptr);

    /** Wide enough for a tab title and a status side by side. */
    QSize sizeHint() const override;

private:
    void updatePlaceholder();

    ClaudeSessionsModel *_model;
    QListView *_list;
    QStackedLayout *_stack;
};

#endif // CLAUDESESSIONSWIDGET_H
