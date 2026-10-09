/*
    SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALPROGRESS_H
#define TERMINALPROGRESS_H

namespace Konsole
{
/**
 * Progress reported by a program with the OSC 9;4 escape sequence.
 */
struct TerminalProgress {
    enum State {
        Normal,
        Error,
        Indeterminate,
        Paused,
    };

    State state = Normal;
    /** Percent complete, 0-100. Not used by the Indeterminate state. */
    int percent = 0;

    bool operator==(const TerminalProgress &other) const
    {
        return state == other.state && percent == other.percent;
    }
    bool operator!=(const TerminalProgress &other) const
    {
        return !(*this == other);
    }
};
}

#endif
