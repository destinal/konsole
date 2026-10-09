// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-FileCopyrightText: George Nachman and the iTerm2 authors
// SPDX-License-Identifier: GPL-2.0-only
//
// Ported from iTerm2's iTermProgramStatus.swift.

#ifndef PROGRAMSTATUSRECORDS_H
#define PROGRAMSTATUSRECORDS_H

#include "ProgramStatus.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <optional>

class SessionStatus;
struct StatusUpdate;

/**
 * The Program Status Protocol (OSC 7501) records a session holds, one per
 * id. Each report replaces its record completely. A port of iTerm2's
 * ProgramStatusRecords.
 */
class ProgramStatusRecords
{
public:
    struct Record {
        Konsole::ProgramStatusReport report;
        /** When the record was last written, for eviction and for breaking ties. */
        int serial = 0;
    };

    static constexpr int defaultCapacity = 256;

    explicit ProgramStatusRecords(int capacity = defaultCapacity);

    bool isEmpty() const;
    int size() const;
    std::optional<Record> record(const QStringList &id) const;

    /** Applies a report: replaces its record, or for Clear removes it and its descendants. */
    void apply(const Konsole::ProgramStatusReport &report);

    /** Removes the records in any of @p states. Returns whether anything was removed. */
    bool remove(const QSet<Konsole::ProgramStatusReport::State> &states);

    void removeAll();

    /** Turns the records in any of @p states idle, keeping their text. Returns whether anything changed. */
    bool markSeen(const QSet<Konsole::ProgramStatusReport::State> &states);

    /**
     * The record most in need of the user (blocked, error, done, working,
     * idle), the most recently written among equals, with its app inherited
     * from its nearest ancestor when it has none.
     */
    std::optional<Record> mostUrgent() const;

private:
    std::optional<QString> inheritedApp(QStringList id) const;

    const int _capacity;
    QHash<QString, Record> _records;
    int _serial = 0;
};

/**
 * Shows a session's OSC 7501 records through its SessionStatus, which they
 * share with the hooks and OSC 21337: whichever wrote last is what shows.
 * A port of the status half of iTerm2's ProgramStatusController.
 */
class ProgramStatusController : public QObject
{
    Q_OBJECT

public:
    explicit ProgramStatusController(SessionStatus *status, QObject *parent = nullptr);

    const ProgramStatusRecords &records() const;

    /** The program sent a report. */
    void handle(const Konsole::ProgramStatusReport &report);

    /**
     * A new shell prompt began: working, blocked and idle records end with
     * the command; done and error stay until the user has seen them.
     */
    void promptStarted();

    /** A full reset removes every record. */
    void terminalReset();

    /** The session's program exited. */
    void programExited();

    /** The user pressed a key: done and error records have been seen and become idle. */
    void userDidPressKey();

    /** The status update that shows @p record. Every field is set or cleared. */
    static StatusUpdate statusUpdate(const ProgramStatusRecords::Record &record);

    /** Removes invisible formatting characters and line separators from text shown outside the terminal. */
    static QString sanitized(const QString &text);

private:
    void refresh(bool showingStatus);
    void otherWriterDidSetStatus();

    SessionStatus *_status;
    ProgramStatusRecords _records;
    // Whether the status showing came from a record. Once something else
    // writes the status, changes the program did not report (a prompt, a key
    // press) must not overwrite it.
    bool _isShowingStatus = false;
    bool _applying = false;
};

#endif // PROGRAMSTATUSRECORDS_H
