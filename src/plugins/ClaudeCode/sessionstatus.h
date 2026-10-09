// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SESSIONSTATUS_H
#define SESSIONSTATUS_H

#include "TabStatus.h"
#include "TerminalProgress.h"

#include <QColor>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <optional>

/**
 * One change to a session's status, from `it2 set-status` (through D-Bus)
 * or from OSC 21337.
 *
 * The visible fields follow TabStatusFields: std::nullopt leaves a field
 * alone, an empty value clears it. The rest is bookkeeping that cc-status
 * parks here between hook events, preconditions that drop the whole update
 * when they do not hold, and what to show when the operation the status
 * belongs to ends.
 */
struct StatusUpdate {
    Konsole::TabStatusFields fields;

    std::optional<qint64> backgroundTasks;
    std::optional<qint64> backgroundTasksDelta;
    std::optional<bool> turnOpen;

    std::optional<bool> requiredTurnOpen;
    std::optional<qint64> requiredBackgroundTasks;

    bool expiresOnProgressEnd = false;
    Konsole::TabStatusFields fallback;

    /**
     * Whether the update says anything about what the session is doing, as
     * opposed to only parking bookkeeping such as the task count. Only such
     * an assertion replaces an armed expiration.
     */
    bool assertsStatus() const;

    static StatusUpdate fromTabStatus(const Konsole::TabStatusUpdate &update);

    /**
     * Builds an update from `it2 set-status` arguments, keyed as iTerm2's
     * set_session_status takes them: status, text_color, dot_color, detail,
     * background_tasks, background_tasks_delta, turn_open, if_turn_open,
     * if_background_tasks, expires_on and then_status, then_text_color,
     * then_dot_color, then_detail. Returns std::nullopt and sets @p error for
     * arguments that make no sense.
     */
    static std::optional<StatusUpdate> fromArguments(const QVariantMap &arguments, QString *error);
};

/**
 * A session's status as cc-status reports it, and when a status scoped to an
 * operation stops being true. A port of iTerm2's iTermSessionTabStatus and
 * TabStatusController.
 *
 * A status can say it holds only until the program's current operation
 * (announced with OSC 9;4 progress) ends, because an interrupt may run no hook
 * that could report the change. When the operation ends the status gives way
 * to its fallback, after a head start that lets the program's own, slower
 * report win, and never while the program still counts background work.
 */
class SessionStatus : public QObject
{
    Q_OBJECT

public:
    /** How long an ended operation's status waits before expiring. */
    static constexpr int defaultProgressEndDelayMs = 3000;

    explicit SessionStatus(QObject *parent = nullptr, int progressEndDelayMs = defaultProgressEndDelayMs);

    std::optional<QString> statusText() const;
    /** Invalid when there is no indicator. */
    QColor indicator() const;
    /** Invalid when there is no status text color. */
    QColor statusColor() const;
    std::optional<QString> detail() const;
    qint64 backgroundTasks() const;
    std::optional<bool> turnOpen() const;

    /** Whether there is a status to show: an indicator or status text. */
    bool hasActiveStatus() const;
    bool hasArmedExpiration() const;

    /**
     * Applies an update. Returns false when its preconditions did not hold,
     * in which case nothing changed.
     */
    bool apply(const StatusUpdate &update);

    /** The program reported progress with OSC 9;4. */
    void progressReported(const std::optional<Konsole::TerminalProgress> &progress);

    /** The user pressed a key: a "done" or "error" status has been seen and becomes idle. */
    void userDidPressKey();

    /** Clears everything. Returns whether there was anything to clear. */
    bool clearStatus();

    /** The session's program exited: forget the status and the operation. */
    void programDidExit();

    /**
     * The status as `it2 set-status --json` and `it2 session get-status`
     * print it: status, text_color, dot_color, detail (null when unset),
     * background_tasks and turn_open (null when never reported).
     */
    QJsonObject toJson() const;

Q_SIGNALS:
    /** Something visible changed. */
    void changed(const std::optional<QString> &previousStatusText);

    /**
     * An update that says something about what the session is doing was
     * applied, as opposed to one that only parked bookkeeping. Emitted
     * whether or not anything visible changed.
     */
    void asserted();

private:
    struct Display {
        QColor indicator;
        std::optional<QString> statusText;
        QColor statusColor;
        std::optional<QString> detail;

        bool operator==(const Display &other) const
        {
            return indicator == other.indicator && statusText == other.statusText && statusColor == other.statusColor && detail == other.detail;
        }
    };

    bool preconditionsHold(const StatusUpdate &update) const;
    /** Applies the fields and bookkeeping, returning whether anything visible changed. */
    bool applyToState(const StatusUpdate &update);
    bool applyFields(const Konsole::TabStatusFields &fields);
    void expire(int scheduledGeneration);
    void expireNow();

    const int _progressEndDelayMs;

    Display _display;
    qint64 _backgroundTasks = 0;
    std::optional<bool> _turnOpen;

    // What the current status asked to show when its operation ends.
    std::optional<Konsole::TabStatusFields> _armedFallback;
    // Bumped whenever a scheduled expiration stops being valid.
    int _generation = 0;
    // Whether the last progress report said something was running; a stop
    // only ends an operation that was seen to start.
    bool _progressWasRunning = false;
    // Whether the head start after the last operation ended has elapsed, and
    // the generation as of that end.
    bool _headStartElapsed = false;
    int _operationEndGeneration = 0;
};

#endif // SESSIONSTATUS_H
