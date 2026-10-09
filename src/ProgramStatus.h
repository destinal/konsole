/*
    SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROGRAMSTATUS_H
#define PROGRAMSTATUS_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>

#include "konsoleprivate_export.h"

namespace Konsole
{
/**
 * One report of the Program Status Protocol (OSC 7501), in which a program
 * says what it is doing: idle, working, blocked on the user, done, or failed.
 *
 * printf '\e]7501;state=working:app=brew:progress=30:msg=%s\e\\' "$(printf 'Installing' | base64)"
 *
 * The body is colon-separated key=value pairs. A program may hold several
 * records, one per id; a report replaces its record completely. Querying
 * with the body "?" asks whether the terminal supports the protocol.
 *
 * Spec: https://www.superlogical.com/rex/docs/build/program-status
 */
struct KONSOLEPRIVATE_EXPORT ProgramStatusReport {
    enum State {
        Idle,
        Working,
        Done,
        Blocked,
        Error,
        /** Removes the record and its descendants; with no id, every record. */
        Clear,
    };

    enum Kind {
        Permission,
        Question,
        Auth,
    };

    State state = Idle;
    /** Path segments of the record's id. Empty for the root record. */
    QStringList id;
    /** Why the program is blocked. Only set for Blocked. */
    std::optional<Kind> kind;
    /** Percent complete, 0-100. Only set for Working and Blocked. */
    std::optional<int> progress;
    std::optional<QString> app;
    /** Decoded from base64. */
    std::optional<QString> title;
    /** Decoded from base64. */
    std::optional<QString> msg;

    /** Whether @p body is the feature detection query. */
    static bool isFeatureQuery(QStringView body);

    /**
     * Parses the body of a report (what follows "7501;"). Returns
     * std::nullopt when the spec says to ignore or discard the report.
     */
    static std::optional<ProgramStatusReport> parse(QStringView body);

    bool operator==(const ProgramStatusReport &other) const;
};
}

Q_DECLARE_METATYPE(Konsole::ProgramStatusReport)

#endif
