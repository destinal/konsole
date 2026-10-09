// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "workgroupswitcher.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

WorkgroupSwitcher::WorkgroupSwitcher(const QStringList &members, QWidget *parent)
    : QWidget(parent)
    , _buttons(new QButtonGroup(this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(0);
    for (int i = 0; i < members.size(); i++) {
        auto *button = new QToolButton(this);
        button->setText(members.at(i));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        _buttons->addButton(button, i);
        layout->addWidget(button);
    }
    _buttons->setExclusive(true);
    connect(_buttons, &QButtonGroup::idClicked, this, &WorkgroupSwitcher::memberChosen);
}

void WorkgroupSwitcher::setCurrent(int member)
{
    if (QAbstractButton *button = _buttons->button(member)) {
        button->setChecked(true);
    }
}

#include "moc_workgroupswitcher.cpp"
