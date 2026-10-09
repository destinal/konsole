// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionstatusregistry.h"
#include "sessionstatus.h"

#include "Emulation.h"
#include "session/Session.h"
#include "session/SessionManager.h"

SessionStatusRegistry::SessionStatusRegistry(QObject *parent)
    : QObject(parent)
{
    auto *manager = Konsole::SessionManager::instance();
    const auto existing = manager->sessions();
    for (Konsole::Session *session : existing) {
        track(session);
    }
    connect(manager, &Konsole::SessionManager::sessionCreated, this, &SessionStatusRegistry::track);
}

SessionStatus *SessionStatusRegistry::status(Konsole::Session *session) const
{
    return _statuses.value(session);
}

Konsole::Session *SessionStatusRegistry::session(const QString &identifier) const
{
    const QLatin1String prefix("/Sessions/");
    bool ok = false;
    const int id = (identifier.startsWith(prefix) ? identifier.mid(prefix.size()) : identifier).toInt(&ok);
    if (!ok) {
        return nullptr;
    }
    for (auto it = _statuses.cbegin(); it != _statuses.cend(); ++it) {
        if (it.key()->sessionId() == id) {
            return it.key();
        }
    }
    return nullptr;
}

QList<Konsole::Session *> SessionStatusRegistry::sessions() const
{
    return _statuses.keys();
}

void SessionStatusRegistry::track(Konsole::Session *session)
{
    if (_statuses.contains(session)) {
        return;
    }
    auto *status = new SessionStatus(this);
    _statuses.insert(session, status);

    connect(session->emulation(), &Konsole::Emulation::progressChanged, status, &SessionStatus::progressReported);
    connect(session, &Konsole::Session::tabStatusReceived, status, [status](const Konsole::TabStatusUpdate &update) {
        status->apply(StatusUpdate::fromTabStatus(update));
    });
    connect(session, &Konsole::Session::finished, status, &SessionStatus::programDidExit);
    connect(status, &SessionStatus::changed, this, [this, session] {
        Q_EMIT statusChanged(session);
    });
    connect(session, &QObject::destroyed, this, [this, session] {
        delete _statuses.take(session);
    });
}

#include "moc_sessionstatusregistry.cpp"
