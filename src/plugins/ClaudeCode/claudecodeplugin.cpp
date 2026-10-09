// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodeplugin.h"
#include "claudecodedbus.h"
#include "claudesessionswidget.h"
#include "konsoledebug.h"

#include "MainWindow.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QDockWidget>

K_PLUGIN_CLASS_WITH_JSON(ClaudeCodePlugin, "konsole_claudecode.json")

struct ClaudeCodePlugin::Private {
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

ClaudeCodePlugin::ClaudeCodePlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    setName(QStringLiteral("ClaudeCode"));

    auto *dbus = new ClaudeCodeDBus(this);
    if (!dbus->registerObject()) {
        qCWarning(KonsoleDebug) << "Claude Code plugin could not register /ClaudeCode on D-Bus";
    }
}

ClaudeCodePlugin::~ClaudeCodePlugin() = default;

void ClaudeCodePlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    auto *dock = new QDockWidget(mainWindow);
    dock->setWindowTitle(i18n("Claude Sessions"));
    dock->setWidget(new ClaudeSessionsWidget(dock));
    dock->setObjectName(QStringLiteral("ClaudeCodeDock"));
    dock->setVisible(false);
    dock->setAllowedAreas(Qt::DockWidgetArea::LeftDockWidgetArea | Qt::DockWidgetArea::RightDockWidgetArea);

    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    priv->dockForWindow[mainWindow] = dock;
    connect(mainWindow, &QObject::destroyed, this, [this, mainWindow] {
        priv->dockForWindow.remove(mainWindow);
    });
}

void ClaudeCodePlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_UNUSED(controller)
    Q_UNUSED(mainWindow)
}

QList<QAction *> ClaudeCodePlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    auto *toggleVisibilityAction = new QAction(i18n("Show Claude Sessions"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-show-sessions"), toggleVisibilityAction);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F3));
    connect(toggleVisibilityAction, &QAction::triggered, priv->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(priv->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

#include "claudecodeplugin.moc"
#include "moc_claudecodeplugin.cpp"
