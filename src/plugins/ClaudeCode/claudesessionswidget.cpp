// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudesessionswidget.h"

#include <KLocalizedString>

#include <QLabel>
#include <QVBoxLayout>

ClaudeSessionsWidget::ClaudeSessionsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(i18n("No Claude Code sessions"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}

#include "moc_claudesessionswidget.cpp"
