// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionnavigation.h"

#include "MainWindow.h"
#include "ViewManager.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KWindowSystem>

#include <QWindow>

namespace ClaudeCode
{
void activateSession(Konsole::Session *session, const QString &activationToken)
{
    const auto views = session->views();
    if (views.isEmpty()) {
        return;
    }
    QWidget *window = views.first()->window();
    if (auto *mainWindow = qobject_cast<Konsole::MainWindow *>(window)) {
        mainWindow->viewManager()->setCurrentSession(session->sessionId());
    }
    window->show();
    window->raise();
    if (!activationToken.isEmpty()) {
        KWindowSystem::setCurrentXdgActivationToken(activationToken);
    }
    if (window->windowHandle() != nullptr) {
        KWindowSystem::activateWindow(window->windowHandle());
    }
    window->activateWindow();
}

bool isSessionFocused(Konsole::Session *session)
{
    const auto views = session->views();
    for (const Konsole::TerminalDisplay *view : views) {
        if (view->hasFocus() && view->window()->isActiveWindow()) {
            return true;
        }
    }
    return false;
}

QString sessionTitle(Konsole::Session *session)
{
    const auto views = session->views();
    for (Konsole::TerminalDisplay *view : views) {
        if (view->sessionController() != nullptr && !view->sessionController()->title().isEmpty()) {
            return view->sessionController()->title();
        }
    }
    return session->nameTitle();
}
}
