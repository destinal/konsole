/*
    SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
    SPDX-FileCopyrightText: George Nachman and the iTerm2 authors

    SPDX-License-Identifier: GPL-2.0-only

    Ported from iTerm2's OSC 21337 parser in VT100Terminal.m.
*/

#include "TabStatus.h"

#include <QStringList>

#include <algorithm>

using namespace Konsole;

namespace
{
bool isHexDigit(QChar ch)
{
    return ch.isDigit() || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')) || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
}

// Parses an xterm color: #RGB, #RRGGBB (and the 12 and 16 bit forms), or
// rgb:R/G/B with 1-4 hex digits per component.
std::optional<QColor> parseColor(const QString &value)
{
    if (value.startsWith(QLatin1Char('#'))) {
        const QColor color = QColor::fromString(value);
        return color.isValid() ? std::optional<QColor>(color) : std::nullopt;
    }

    const QLatin1String rgbPrefix("rgb:");
    if (!value.startsWith(rgbPrefix)) {
        return std::nullopt;
    }
    const auto parts = QStringView(value).mid(rgbPrefix.size()).split(QLatin1Char('/'));
    if (parts.size() != 3) {
        return std::nullopt;
    }
    float components[3];
    for (int i = 0; i < 3; i++) {
        const QStringView part = parts.at(i);
        if (part.isEmpty() || part.size() > 4 || !std::all_of(part.begin(), part.end(), isHexDigit)) {
            return std::nullopt;
        }
        // A component of n hex digits scales from 0 to 16^n - 1.
        const float max = float((1u << (4 * part.size())) - 1);
        components[i] = float(part.toUInt(nullptr, 16)) / max;
    }
    return QColor::fromRgbF(components[0], components[1], components[2]);
}

// Applies one key=value pair to fields. Returns whether it set anything:
// false for a key that is not a status field, or a color that does not
// parse. An empty value clears the field.
bool setField(TabStatusFields &fields, QStringView key, const QString &value)
{
    if (key == QLatin1String("status")) {
        fields.status = value;
    } else if (key == QLatin1String("detail")) {
        fields.detail = value;
    } else if (key == QLatin1String("indicator") || key == QLatin1String("status-color")) {
        std::optional<QColor> &field = key == QLatin1String("indicator") ? fields.indicator : fields.statusColor;
        if (value.isEmpty()) {
            field = QColor();
        } else {
            const auto color = parseColor(value);
            if (!color.has_value()) {
                return false;
            }
            field = color;
        }
    } else {
        return false;
    }
    return true;
}
}

TabStatusFields TabStatusFields::cleared()
{
    TabStatusFields fields;
    fields.status = QString();
    fields.indicator = QColor();
    fields.statusColor = QColor();
    fields.detail = QString();
    return fields;
}

bool TabStatusFields::operator==(const TabStatusFields &other) const
{
    return status == other.status && indicator == other.indicator && statusColor == other.statusColor && detail == other.detail;
}

TabStatusUpdate TabStatusUpdate::fromOsc21337(QStringView payload)
{
    // Split on semicolons that are not escaped. "\;" and "\\" stand for a
    // semicolon and a backslash; any other escape is kept as written.
    QStringList tokens;
    QString current;
    bool escaped = false;
    for (const QChar ch : payload) {
        if (escaped) {
            if (ch != QLatin1Char(';') && ch != QLatin1Char('\\')) {
                current += QLatin1Char('\\');
            }
            current += ch;
            escaped = false;
        } else if (ch == QLatin1Char('\\')) {
            escaped = true;
        } else if (ch == QLatin1Char(';')) {
            tokens.append(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    tokens.append(current);

    const QLatin1String thenPrefix("then-");
    TabStatusUpdate update;
    TabStatusFields fallback;
    bool hasFallback = false;
    for (const QString &token : std::as_const(tokens)) {
        const qsizetype separator = token.indexOf(QLatin1Char('='));
        const QStringView key = separator < 0 ? QStringView(token) : QStringView(token).left(separator);
        const QString value = separator < 0 ? QString() : token.mid(separator + 1);

        if (key == QLatin1String("expires-on")) {
            if (value == QLatin1String("progress-end")) {
                update.expiresOnProgressEnd = true;
            } else if (value.isEmpty()) {
                update.expiresOnProgressEnd = false;
            }
            continue;
        }
        // A then- key that sets nothing must not create a fallback, or a
        // typo would turn "expire and clear" into "expire and do nothing".
        if (key.startsWith(thenPrefix)) {
            hasFallback = setField(fallback, key.mid(thenPrefix.size()), value) || hasFallback;
            continue;
        }
        setField(update.fields, key, value);
    }

    if (update.expiresOnProgressEnd) {
        update.fallback = hasFallback ? fallback : TabStatusFields::cleared();
    }
    return update;
}

bool TabStatusUpdate::operator==(const TabStatusUpdate &other) const
{
    return fields == other.fields && expiresOnProgressEnd == other.expiresOnProgressEnd && fallback == other.fallback;
}
