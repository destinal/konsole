// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodeplugin.h"
#include "claudecodedbus.h"
#include "claudesessionswidget.h"
#include "konsoledebug.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"

#include "MainWindow.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QDockWidget>

K_PLUGIN_CLASS_WITH_JSON(ClaudeCodePlugin, "konsole_claudecode.json")

struct ClaudeCodePlugin::Private {
    SessionStatusRegistry *registry = nullptr;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

ClaudeCodePlugin::ClaudeCodePlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    setName(QStringLiteral("ClaudeCode"));

    priv->registry = new SessionStatusRegistry(this);

    // Claude Code reports a turn's start and end with OSC 9;4 progress only to
    // terminals it recognizes, ConEmu among them; without the end of a turn,
    // an interrupt (which runs no hook) leaves the status on "working". Until
    // Konsole speaks Claude Code's own status protocol (OSC 7501), pass as
    // ConEmu. This must happen before the session's program starts.
    connect(Konsole::SessionManager::instance(), &Konsole::SessionManager::sessionCreated, this, [](Konsole::Session *session) {
        session->addEnvironmentEntry(QStringLiteral("ConEmuANSI=ON"));
    });
    auto *dbus = new ClaudeCodeDBus(priv->registry, this);
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
    Q_UNUSED(mainWindow)
    if (controller == nullptr || controller->view() == nullptr) {
        return;
    }
    // A key press in a session showing a finished or failed turn means the
    // user has seen it. Only the active view takes keys, so connecting here
    // covers every view that ever does.
    SessionStatus *status = priv->registry->status(controller->session());
    if (status != nullptr) {
        connect(controller->view(), &Konsole::TerminalDisplay::keyPressedSignal, status, &SessionStatus::userDidPressKey, Qt::UniqueConnection);
    }
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
