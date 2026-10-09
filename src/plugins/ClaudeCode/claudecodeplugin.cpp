// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodeplugin.h"
#include "claudecodedbus.h"
#include "claudesessionsmodel.h"
#include "claudesessionswidget.h"
#include "clippingstore.h"
#include "clippingswidget.h"
#include "konsoledebug.h"
#include "programstatusrecords.h"
#include "sessionnavigation.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"
#include "statusbadges.h"
#include "statusnotifier.h"
#include "workgroupmanager.h"

#include "MainWindow.h"
#include "ViewManager.h"
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
    WorkgroupManager *workgroups = nullptr;
    ClippingStore *clippings = nullptr;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> clippingsDockForWindow;
    QMap<Konsole::MainWindow *, ClippingsWidget *> clippingsForWindow;
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
    priv->clippings = new ClippingStore(this);
    priv->workgroups = new WorkgroupManager(priv->registry, priv->clippings, this);

    // New findings bring up the Clippings panel of each window showing them.
    connect(priv->clippings, &ClippingStore::added, this, [this](Konsole::Session *owner) {
        for (auto it = priv->clippingsForWindow.cbegin(); it != priv->clippingsForWindow.cend(); ++it) {
            if (it.value()->owner() == owner) {
                priv->clippingsDockForWindow.value(it.key())->show();
            }
        }
    });

    // The session's tab shows its status as a colored dot.
    connect(priv->registry, &SessionStatusRegistry::statusChanged, this, [this](Konsole::Session *session) {
        SessionStatus *status = priv->registry->status(session);
        session->setStatusIndicator(status != nullptr && status->hasActiveStatus() ? status->indicator() : QColor());
    });
    auto *dbus = new ClaudeCodeDBus(priv->registry, priv->clippings, this);
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

    auto *clippingsDock = new QDockWidget(mainWindow);
    clippingsDock->setWindowTitle(i18n("Claude Clippings"));
    auto *clippingsWidget = new ClippingsWidget(priv->clippings, clippingsDock);
    clippingsDock->setWidget(clippingsWidget);
    clippingsDock->setObjectName(QStringLiteral("ClaudeCodeClippingsDock"));
    clippingsDock->setAllowedAreas(Qt::DockWidgetArea::LeftDockWidgetArea | Qt::DockWidgetArea::RightDockWidgetArea);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, clippingsDock);
    clippingsDock->hide();
    priv->clippingsDockForWindow[mainWindow] = clippingsDock;
    priv->clippingsForWindow[mainWindow] = clippingsWidget;
    // Inside a workgroup, Chat is shown by switching to it.
    connect(clippingsWidget, &ClippingsWidget::sentToChat, this, [this](Konsole::Session *owner) {
        if (Workgroup *workgroup = priv->workgroups->workgroup(owner)) {
            workgroup->show(Workgroup::Chat);
        } else {
            ClaudeCode::activateSession(owner);
        }
    });

    connect(mainWindow, &QObject::destroyed, this, [this, mainWindow] {
        priv->dockForWindow.remove(mainWindow);
        priv->clippingsDockForWindow.remove(mainWindow);
        priv->clippingsForWindow.remove(mainWindow);
    });
}

void ClaudeCodePlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    if (controller == nullptr || controller->view() == nullptr) {
        return;
    }
    // The Clippings panel shows the findings of the workgroup in view, kept
    // by its Chat session, or else those of the session itself.
    if (ClippingsWidget *clippings = priv->clippingsForWindow.value(mainWindow)) {
        Konsole::Session *session = controller->session();
        Workgroup *workgroup = priv->workgroups->workgroup(session);
        clippings->setOwner(workgroup != nullptr ? workgroup->session(Workgroup::Chat) : session);
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

    // Workgroups act on the window's active session.
    auto activeSession = [mainWindow]() -> Konsole::Session * {
        Konsole::SessionController *controller = mainWindow->viewManager()->activeViewController();
        return controller != nullptr ? controller->session().data() : nullptr;
    };

    auto *enterAction = new QAction(i18n("Enter Claude Workgroup"), mainWindow);
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-enter-workgroup"), enterAction);
    connect(enterAction, &QAction::triggered, priv->workgroups, [this, activeSession] {
        priv->workgroups->enter(activeSession());
    });

    auto *exitAction = new QAction(i18n("Exit Claude Workgroup"), mainWindow);
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-exit-workgroup"), exitAction);
    connect(exitAction, &QAction::triggered, priv->workgroups, [this, activeSession] {
        priv->workgroups->exit(activeSession());
    });

    QList<QAction *> memberActions;
    const QStringList names = Workgroup::memberNames();
    for (int member = 0; member < Workgroup::memberCount; member++) {
        auto *action = new QAction(i18nc("@action show a workgroup member", "Show %1", names.at(member)), mainWindow);
        mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-workgroup-member-%1").arg(member), action);
        mainWindow->actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key(Qt::Key_1 + member)));
        connect(action, &QAction::triggered, priv->workgroups, [this, activeSession, member] {
            if (Workgroup *workgroup = priv->workgroups->workgroup(activeSession())) {
                workgroup->show(Workgroup::Member(member));
            }
        });
        memberActions.append(action);
    }

    QAction *clippingsAction = priv->clippingsDockForWindow[mainWindow]->toggleViewAction();
    clippingsAction->setText(i18n("Show Claude Clippings"));
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-show-clippings"), clippingsAction);
    mainWindow->actionCollection()->setDefaultShortcut(clippingsAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F4));

    auto *autoSendAction = new QAction(i18n("Send Review Findings to Chat Automatically"), mainWindow);
    autoSendAction->setCheckable(true);
    autoSendAction->setChecked(priv->workgroups->autoSend());
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-auto-send-clippings"), autoSendAction);
    connect(autoSendAction, &QAction::toggled, priv->workgroups, &WorkgroupManager::setAutoSend);
    connect(priv->workgroups, &WorkgroupManager::autoSendChanged, autoSendAction, &QAction::setChecked);

    auto *autoEnterAction = new QAction(i18n("Enter Claude Workgroups Automatically"), mainWindow);
    autoEnterAction->setCheckable(true);
    autoEnterAction->setChecked(priv->workgroups->autoEnter());
    mainWindow->actionCollection()->addAction(QStringLiteral("claudecode-auto-enter-workgroup"), autoEnterAction);
    connect(autoEnterAction, &QAction::toggled, priv->workgroups, &WorkgroupManager::setAutoEnter);
    connect(priv->workgroups, &WorkgroupManager::autoEnterChanged, autoEnterAction, &QAction::setChecked);

    return QList<QAction *>{toggleVisibilityAction, clippingsAction, badgesAction, enterAction, exitAction} + memberActions
        + QList<QAction *>{autoEnterAction, autoSendAction};
}

#include "claudecodeplugin.moc"
#include "moc_claudecodeplugin.cpp"
