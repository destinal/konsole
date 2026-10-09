/*
    SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TABSTATUS_H
#define TABSTATUS_H

#include <QColor>
#include <QMetaType>
#include <QString>
#include <QStringView>

#include <optional>

#include "konsoleprivate_export.h"

namespace Konsole
{
/**
 * Fields of a tab status, as set by the iTerm2 escape sequence OSC 21337.
 *
 * Each field is std::nullopt when the update leaves it unchanged. A field
 * that is set but empty (an empty string, an invalid color) clears it.
 */
struct KONSOLEPRIVATE_EXPORT TabStatusFields {
    std::optional<QString> status;
    std::optional<QColor> indicator;
    std::optional<QColor> statusColor;
    std::optional<QString> detail;

    /** Returns fields that clear every part of the status. */
    static TabStatusFields cleared();

    bool operator==(const TabStatusFields &other) const;
    bool operator!=(const TabStatusFields &other) const
    {
        return !(*this == other);
    }
};

/**
 * A tab status update received through OSC 21337.
 *
 * printf '\e]21337;status=Working;indicator=#ffa500\a'
 *
 * The payload is semicolon-separated key=value pairs, where "\;" and "\\"
 * escape a semicolon and a backslash. Keys: status, indicator (a color),
 * status-color, detail; colors are #RRGGBB or xterm rgb:R/G/B. With
 * expires-on=progress-end the status holds only until the program's OSC 9;4
 * progress ends, and then-<key> fields name what replaces it (by default,
 * nothing: the status is cleared). Unknown keys are ignored.
 */
struct KONSOLEPRIVATE_EXPORT TabStatusUpdate {
    TabStatusFields fields;
    bool expiresOnProgressEnd = false;
    /** Applied when the status expires. Only meaningful with expiresOnProgressEnd. */
    TabStatusFields fallback;

    static TabStatusUpdate fromOsc21337(QStringView payload);

    bool operator==(const TabStatusUpdate &other) const;
    bool operator!=(const TabStatusUpdate &other) const
    {
        return !(*this == other);
    }
};
}

Q_DECLARE_METATYPE(Konsole::TabStatusUpdate)

#endif
