// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodedbus.h"

#include "session/Session.h"
#include "session/SessionManager.h"

#include <QDBusConnection>

ClaudeCodeDBus::ClaudeCodeDBus(QObject *parent)
    : QObject(parent)
{
}

bool ClaudeCodeDBus::registerObject()
{
    return QDBusConnection::sessionBus().registerObject(QStringLiteral("/ClaudeCode"), this, QDBusConnection::ExportAllSlots);
}

QString ClaudeCodeDBus::ping() const
{
    return QStringLiteral(RELEASE_SERVICE_VERSION);
}

QStringList ClaudeCodeDBus::sessions() const
{
    QStringList paths;
    const auto sessions = Konsole::SessionManager::instance()->sessions();
    for (const Konsole::Session *session : sessions) {
        paths.append(QStringLiteral("/Sessions/%1").arg(session->sessionId()));
    }
    return paths;
}

#include "moc_claudecodedbus.cpp"
