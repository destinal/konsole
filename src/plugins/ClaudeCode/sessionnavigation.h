// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SESSIONNAVIGATION_H
#define SESSIONNAVIGATION_H

#include <QString>

namespace Konsole
{
class Session;
}

namespace ClaudeCode
{
/**
 * Brings @p session's window to the front and focuses its view. On Wayland,
 * @p activationToken (from a notification action, say) lets another window
 * take focus.
 */
void activateSession(Konsole::Session *session, const QString &activationToken = QString());

/** Whether one of @p session's views has focus in the active window. */
bool isSessionFocused(Konsole::Session *session);

/** The title shown on the session's tab, or its name when it has no view. */
QString sessionTitle(Konsole::Session *session);

/**
 * Types @p text into @p session the way a paste does: bracketed when the
 * program asked for bracketed paste, line breaks as carriage returns, no
 * escape characters. With @p submit, presses Return a moment later, once the
 * program has taken the paste in.
 */
void pasteIntoSession(Konsole::Session *session, QString text, bool submit);
}

#endif // SESSIONNAVIGATION_H
