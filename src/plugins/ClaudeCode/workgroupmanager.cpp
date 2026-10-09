// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "workgroupmanager.h"
#include "sessionstatusregistry.h"

#include "ProgramStatus.h"
#include "session/Session.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

namespace
{
KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("ClaudeCode"));
}

// After Claude Code clears its status, how often and for how long to look for
// its exit: it may still be running its SessionEnd hooks.
constexpr int exitCheckIntervalMs = 250;
constexpr int exitCheckAttempts = 40;
}

WorkgroupManager::WorkgroupManager(SessionStatusRegistry *registry, QObject *parent)
    : QObject(parent)
    , _autoEnter(settings().readEntry("AutoEnterWorkgroup", true))
{
    connect(registry, &SessionStatusRegistry::programStatusReported, this, &WorkgroupManager::programStatusReported);
}

QString WorkgroupManager::toolsDirectory()
{
    // A build tree keeps them next to Konsole; an installation, in libexec.
    const QString beside = QCoreApplication::applicationDirPath() + QLatin1String("/claudecode");
    if (QFileInfo::exists(beside + QLatin1String("/claudecode-diff"))) {
        return beside;
    }
    return QStringLiteral(CLAUDECODE_TOOLS_DIRECTORY);
}

Workgroup *WorkgroupManager::workgroup(Konsole::Session *session) const
{
    for (Workgroup *workgroup : _workgroups) {
        if (workgroup->contains(session)) {
            return workgroup;
        }
    }
    return nullptr;
}

void WorkgroupManager::enter(Konsole::Session *session)
{
    if (session == nullptr || workgroup(session) != nullptr || session->views().isEmpty()) {
        return;
    }
    auto *workgroup = new Workgroup(session, toolsDirectory(), this);
    _workgroups.append(workgroup);
    connect(workgroup, &Workgroup::dissolved, this, [this, workgroup] {
        _workgroups.removeOne(workgroup);
        workgroup->deleteLater();
    });
}

void WorkgroupManager::exit(Konsole::Session *session)
{
    if (Workgroup *workgroup = this->workgroup(session)) {
        workgroup->dissolve();
    }
}

bool WorkgroupManager::autoEnter() const
{
    return _autoEnter;
}

void WorkgroupManager::setAutoEnter(bool enabled)
{
    if (_autoEnter == enabled) {
        return;
    }
    _autoEnter = enabled;
    KConfigGroup group = settings();
    group.writeEntry("AutoEnterWorkgroup", enabled);
    group.sync();
    Q_EMIT autoEnterChanged(enabled);
}

void WorkgroupManager::checkForExit(const QPointer<Konsole::Session> &session, int attemptsLeft)
{
    QTimer::singleShot(exitCheckIntervalMs, this, [this, session, attemptsLeft] {
        if (session == nullptr || workgroup(session) == nullptr) {
            return;
        }
        // Nothing but the shell left in the foreground: Claude Code is gone.
        if (session->foregroundProcessId() == session->processId()) {
            exit(session);
        } else if (attemptsLeft > 1) {
            checkForExit(session, attemptsLeft - 1);
        }
    });
}

void WorkgroupManager::programStatusReported(Konsole::Session *session, const Konsole::ProgramStatusReport &report)
{
    if (!report.id.isEmpty()) {
        return;
    }
    QPointer<Konsole::Session> guarded(session);
    Workgroup *current = workgroup(session);

    if (report.state == Konsole::ProgramStatusReport::Clear) {
        // Claude Code clears its records when it exits; a clear carries no
        // app. Only the Chat member's Claude Code ends the workgroup, and only
        // once nothing runs in the session's shell any more.
        if (current == nullptr || current->session(Workgroup::Chat) != session) {
            return;
        }
        checkForExit(guarded, exitCheckAttempts);
        return;
    }

    if (_autoEnter && current == nullptr && report.app == QLatin1String("claude-code")) {
        // Not from inside the terminal's escape sequence handling.
        QTimer::singleShot(0, this, [this, guarded] {
            if (guarded != nullptr) {
                enter(guarded);
            }
        });
    }
}

#include "moc_workgroupmanager.cpp"
