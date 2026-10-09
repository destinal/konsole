// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WORKGROUP_H
#define WORKGROUP_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <array>

namespace Konsole
{
class Session;
class TerminalDisplay;
}
class WorkgroupSwitcher;

/**
 * A Claude Code workgroup, after iTerm2's: the session running Claude Code
 * (Chat) and two members beside it, Diff and Code Review. They share the
 * Chat session's tab as splits, one maximized at a time; a switcher in each
 * member's header bar, or a shortcut, shows another.
 *
 * The members start nothing on their own: Diff runs its diff the first time
 * it is shown, and Code Review starts a review when asked.
 */
class Workgroup : public QObject
{
    Q_OBJECT

public:
    enum Member {
        Chat,
        Diff,
        Review,
    };
    static constexpr int memberCount = 3;

    /** The members' names, in switcher order. */
    static QStringList memberNames();

    /**
     * Builds a workgroup around @p chat, whose tab gains the other members.
     * @p toolsDirectory holds the member scripts and it2.
     */
    Workgroup(Konsole::Session *chat, const QString &toolsDirectory, QObject *parent = nullptr);
    ~Workgroup() override;

    Konsole::Session *session(Member member) const;
    bool contains(Konsole::Session *session) const;
    Member current() const;

    /** Shows @p member in place of the others. */
    void show(Member member);

    /** Closes the Diff and Code Review members and shows Chat as it was. Emits dissolved(). */
    void dissolve();

Q_SIGNALS:
    void dissolved();

private:
    Konsole::Session *createMember(Member member, const QString &toolsDirectory);
    Konsole::TerminalDisplay *view(Member member) const;
    void addSwitcher(Member member);

    std::array<QPointer<Konsole::Session>, memberCount> _sessions;
    QList<QPointer<WorkgroupSwitcher>> _switchers;
    Member _current = Chat;
    bool _diffStarted = false;
    bool _dissolving = false;
};

#endif // WORKGROUP_H
