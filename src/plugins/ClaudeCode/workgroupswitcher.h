// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WORKGROUPSWITCHER_H
#define WORKGROUPSWITCHER_H

#include <QWidget>

class QButtonGroup;

/**
 * A segmented control naming a workgroup's members (Chat, Diff, Code
 * Review), shown in the header bar of each member's view. Choosing one
 * switches the workgroup to it.
 */
class WorkgroupSwitcher : public QWidget
{
    Q_OBJECT

public:
    explicit WorkgroupSwitcher(const QStringList &members, QWidget *parent = nullptr);

    /** Marks @p member as the one showing, without emitting memberChosen. */
    void setCurrent(int member);

Q_SIGNALS:
    void memberChosen(int member);

private:
    QButtonGroup *_buttons;
};

#endif // WORKGROUPSWITCHER_H
