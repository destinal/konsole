// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionnavigation.h"

#include "MainWindow.h"
#include "ViewManager.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include "Emulation.h"

#include <KWindowSystem>

#include <QKeyEvent>
#include <QPointer>
#include <QTimer>
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

void pasteIntoSession(Konsole::Session *session, QString text, bool submit)
{
    // What TerminalDisplay::doPaste does to pasted text.
    text.replace(QLatin1String("\r\n"), QLatin1String("\r"));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    text.remove(QLatin1Char('\033'));
    const auto views = session->views();
    if (!views.isEmpty() && views.first()->bracketedPasteMode()) {
        text = QLatin1String("\033[200~") + text + QLatin1String("\033[201~");
    }
    session->sendTextToTerminal(text, QChar());

    if (submit) {
        // iTerm2 waits this long before submitting an automatic send.
        QTimer::singleShot(501, session, [session = QPointer<Konsole::Session>(session)] {
            if (session != nullptr) {
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
                session->emulation()->sendKeyEvent(&enter);
            }
        });
    }
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
