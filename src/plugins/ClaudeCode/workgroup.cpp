// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "workgroup.h"
#include "workgroupswitcher.h"

#include "MainWindow.h"
#include "ViewManager.h"
#include "profile/Profile.h"
#include "session/Session.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/TerminalHeaderBar.h"
#include "widgets/ViewContainer.h"
#include "widgets/ViewSplitter.h"

#include <KLocalizedString>

#include <QBoxLayout>
#include <QDBusConnection>
#include <QProcessEnvironment>

QStringList Workgroup::memberNames()
{
    return {i18nc("@action:button workgroup member", "Chat"),
            i18nc("@action:button workgroup member", "Diff"),
            i18nc("@action:button workgroup member", "Code Review")};
}

Workgroup::Workgroup(Konsole::Session *chat, const QString &toolsDirectory, QObject *parent)
    : QObject(parent)
{
    _sessions[Chat] = chat;
    _sessions[Diff] = createMember(Diff, toolsDirectory);
    _sessions[Review] = createMember(Review, toolsDirectory);

    for (int member = 0; member < memberCount; member++) {
        Konsole::Session *session = _sessions[member];
        if (session == nullptr) {
            continue;
        }
        addSwitcher(Member(member));
        // Closing any member ends the workgroup, as in iTerm2.
        connect(session, &QObject::destroyed, this, &Workgroup::dissolve);
    }
    show(Chat);
}

Workgroup::~Workgroup() = default;

Konsole::Session *Workgroup::createMember(Member member, const QString &toolsDirectory)
{
    Konsole::Session *chat = _sessions[Chat];
    Konsole::TerminalDisplay *chatView = view(Chat);
    auto *window = chatView != nullptr ? qobject_cast<Konsole::MainWindow *>(chatView->window()) : nullptr;
    if (window == nullptr) {
        return nullptr;
    }
    Konsole::ViewManager *viewManager = window->viewManager();

    // The members split the Chat session's view, in its directory.
    viewManager->setCurrentSession(chat->sessionId());
    auto profile = Konsole::SessionManager::instance()->sessionProfile(chat);
    Konsole::Session *session = viewManager->createSession(profile, chat->currentWorkingDirectory());

    const QString script = toolsDirectory + (member == Diff ? QLatin1String("/claudecode-diff") : QLatin1String("/claudecode-review"));
    session->setProgram(QStringLiteral("/bin/bash"));
    session->setArguments({QStringLiteral("/bin/bash"), script});
    // A review runs it2 to file its findings with the Chat session.
    const QString path = QProcessEnvironment::systemEnvironment().value(QStringLiteral("PATH"));
    session->addEnvironmentEntry(QStringLiteral("PATH=%1:%2").arg(toolsDirectory, path));
    session->addEnvironmentEntry(
        QStringLiteral("TERM_SESSION_ID=konsole:%1/Sessions/%2").arg(QDBusConnection::sessionBus().baseService()).arg(chat->sessionId()));
    session->setTitle(Konsole::Session::NameRole, memberNames().at(member));

    Konsole::TerminalDisplay *display = viewManager->createView(session);
    viewManager->activeContainer()->splitView(display, Qt::Horizontal);
    return session;
}

Konsole::Session *Workgroup::session(Member member) const
{
    return _sessions[member];
}

bool Workgroup::contains(Konsole::Session *session) const
{
    return std::find(_sessions.begin(), _sessions.end(), session) != _sessions.end();
}

Workgroup::Member Workgroup::current() const
{
    return _current;
}

Konsole::TerminalDisplay *Workgroup::view(Member member) const
{
    Konsole::Session *session = _sessions[member];
    if (session == nullptr || session->views().isEmpty()) {
        return nullptr;
    }
    return session->views().first();
}

void Workgroup::addSwitcher(Member member)
{
    Konsole::TerminalDisplay *display = view(member);
    auto *layout = display != nullptr ? qobject_cast<QBoxLayout *>(display->headerBar()->layout()) : nullptr;
    if (layout == nullptr) {
        return;
    }
    auto *switcher = new WorkgroupSwitcher(memberNames(), display->headerBar());
    // After the session's icon, before its title.
    layout->insertWidget(1, switcher);
    connect(switcher, &WorkgroupSwitcher::memberChosen, this, [this](int chosen) {
        show(Member(chosen));
    });
    _switchers.append(switcher);
}

void Workgroup::show(Member member)
{
    Konsole::TerminalDisplay *display = view(member);
    if (display == nullptr) {
        return;
    }
    Konsole::ViewSplitter *splitter = Konsole::ViewSplitter::parentSplitterForDisplay(display);
    if (splitter == nullptr) {
        return;
    }
    Konsole::ViewSplitter *top = splitter->getToplevelSplitter();
    top->clearMaximized();
    display->setFocus(Qt::OtherFocusReason);
    // Maximizes the focused view, hiding the others.
    top->toggleMaximizeCurrentTerminal();

    _current = member;
    for (const auto &switcher : std::as_const(_switchers)) {
        if (switcher != nullptr) {
            switcher->setCurrent(member);
        }
    }

    // The diff waits to be seen, so that it shows the changes as they are then.
    if (member == Diff && !_diffStarted) {
        _diffStarted = true;
        _sessions[Diff]->sendTextToTerminal(QString(), QLatin1Char('\r'));
    }
}

void Workgroup::dissolve()
{
    if (_dissolving) {
        return;
    }
    _dissolving = true;

    for (const auto &switcher : std::as_const(_switchers)) {
        delete switcher.data();
    }
    _switchers.clear();

    // Show Chat on its own again, then close the others.
    if (Konsole::TerminalDisplay *display = view(Chat)) {
        if (Konsole::ViewSplitter *splitter = Konsole::ViewSplitter::parentSplitterForDisplay(display)) {
            splitter->getToplevelSplitter()->clearMaximized();
        }
        display->setFocus(Qt::OtherFocusReason);
    }
    for (Member member : {Diff, Review}) {
        if (Konsole::Session *session = _sessions[member]) {
            disconnect(session, nullptr, this, nullptr);
            session->closeInNormalWay();
        }
    }
    Q_EMIT dissolved();
}

#include "moc_workgroup.cpp"
