// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WORKGROUPMANAGER_H
#define WORKGROUPMANAGER_H

#include "workgroup.h"

#include <QList>
#include <QObject>
#include <QPointer>

namespace Konsole
{
class Session;
struct ProgramStatusReport;
}
class SessionStatusRegistry;

/**
 * Starts and ends Claude Code workgroups. With automatic entry on (the
 * default, kept in konsolerc as [ClaudeCode] AutoEnterWorkgroup), a session
 * enters a workgroup when Claude Code starts in it, which Claude Code's first
 * program status report (OSC 7501, app=claude-code) shows, and leaves it when
 * Claude Code exits there.
 */
class WorkgroupManager : public QObject
{
    Q_OBJECT

public:
    explicit WorkgroupManager(SessionStatusRegistry *registry, QObject *parent = nullptr);

    /** The workgroup @p session belongs to, or nullptr. */
    Workgroup *workgroup(Konsole::Session *session) const;

    /** Makes @p session the Chat member of a new workgroup. */
    void enter(Konsole::Session *session);

    /** Ends the workgroup @p session belongs to. */
    void exit(Konsole::Session *session);

    bool autoEnter() const;
    void setAutoEnter(bool enabled);

    /** The directory holding the member scripts and it2. */
    static QString toolsDirectory();

Q_SIGNALS:
    void autoEnterChanged(bool enabled);

private:
    void programStatusReported(Konsole::Session *session, const Konsole::ProgramStatusReport &report);
    void checkForExit(const QPointer<Konsole::Session> &session, int attemptsLeft);

    QList<Workgroup *> _workgroups;
    bool _autoEnter;
};

#endif // WORKGROUPMANAGER_H
