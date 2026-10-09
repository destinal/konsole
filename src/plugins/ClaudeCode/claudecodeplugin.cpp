// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodeplugin.h"
#include "claudecodedbus.h"
#include "claudesessionsmodel.h"
#include "claudesessionswidget.h"
#include "konsoledebug.h"
#include "programstatusrecords.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"
#include "statusbadges.h"
#include "statusnotifier.h"

#include "MainWindow.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QDockWidget>
#include <QTimer>

K_PLUGIN_CLASS_WITH_JSON(ClaudeCodePlugin, "konsole_claudecode.json")

struct ClaudeCodePlugin::Private {
    SessionStatusRegistry *registry = nullptr;
    ClaudeSessionsModel *model = nullptr;
    StatusBadges *badges = nullptr;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

ClaudeCodePlugin::ClaudeCodePlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    setName(QStringLiteral("ClaudeCode"));

    priv->registry = new SessionStatusRegistry(this);
    priv->model = new ClaudeSessionsModel(priv->registry, this);
    new StatusNotifier(priv->registry, this);
    priv->badges = new StatusBadges(priv->registry, this);

    // The session's tab shows its status as a colored dot.
    connect(priv->registry, &SessionStatusRegistry::statusChanged, this, [this](Konsole::Session *session) {
        SessionStatus *status = priv->registry->status(session);
        session->setStatusIndicator(status != nullptr && status->hasActiveStatus() ? status->indicator() : QColor());
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
    dock->setWidget(new ClaudeSessionsWidget(priv->model, dock));
    dock->setObjectName(QStringLiteral("ClaudeCodeDock"));
    dock->setAllowedAreas(Qt::DockWidgetArea::LeftDockWidgetArea | Qt::DockWidgetArea::RightDockWidgetArea);

    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    // Hidden until asked for; adding a dock shows it. The window remembers the
    // choice from then on.
    dock->hide();
    // A dock first shown after its window is laid out comes up at its minimum
    // width; give it the room it asks for.
    connect(dock, &QDockWidget::visibilityChanged, dock, [dock, mainWindow](bool visible) {
        const int wanted = dock->widget()->sizeHint().width();
        if (visible && dock->width() < wanted) {
            QTimer::singleShot(0, dock, [dock, mainWindow, wanted] {
                mainWindow->resizeDocks({dock}, {wanted}, Qt::Horizontal);
            });
        }
    });
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
        connect(controller->view(),
                &Konsole::TerminalDisplay::keyPressedSignal,
                priv->registry->programStatus(controller->session()),
                &ProgramStatusController::userDidPressKey,
                Qt::UniqueConnection);
    }
}

QList<QAction *> ClaudeCodePlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    // The dock's own toggle action, which always agrees with whether the dock shows.
    QDockWidget *dock = priv->dockForWindow[mainWindow];
    QAction *toggleVisibilityAction = dock->toggleViewAction();
    toggleVisibilityAction->setText(i18n("Show Claude Sessions"));
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-show-sessions"), toggleVisibilityAction);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F3));

    auto *badgesAction = new QAction(i18n("Show Status Badges"), mainWindow);
    badgesAction->setCheckable(true);
    badgesAction->setChecked(priv->badges->isEnabled());
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-show-badges"), badgesAction);
    connect(badgesAction, &QAction::toggled, priv->badges, &StatusBadges::setEnabled);
    // One setting for every window: keep each window's menu in step.
    connect(priv->badges, &StatusBadges::enabledChanged, badgesAction, &QAction::setChecked);

    return {toggleVisibilityAction, badgesAction};
}

#include "claudecodeplugin.moc"
#include "moc_claudecodeplugin.cpp"
