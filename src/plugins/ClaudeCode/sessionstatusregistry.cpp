// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionstatusregistry.h"
#include "programstatusrecords.h"
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
    return _sessions.value(session).status;
}

ProgramStatusController *SessionStatusRegistry::programStatus(Konsole::Session *session) const
{
    return _sessions.value(session).programStatus;
}

Konsole::Session *SessionStatusRegistry::session(const QString &identifier) const
{
    const QLatin1String prefix("/Sessions/");
    bool ok = false;
    const int id = (identifier.startsWith(prefix) ? identifier.mid(prefix.size()) : identifier).toInt(&ok);
    if (!ok) {
        return nullptr;
    }
    for (auto it = _sessions.cbegin(); it != _sessions.cend(); ++it) {
        if (it.key()->sessionId() == id) {
            return it.key();
        }
    }
    return nullptr;
}

QList<Konsole::Session *> SessionStatusRegistry::sessions() const
{
    return _sessions.keys();
}

void SessionStatusRegistry::track(Konsole::Session *session)
{
    if (_sessions.contains(session)) {
        return;
    }
    auto *status = new SessionStatus(this);
    auto *programStatus = new ProgramStatusController(status, status);
    _sessions.insert(session, {status, programStatus});

    Konsole::Emulation *emulation = session->emulation();
    connect(emulation, &Konsole::Emulation::progressChanged, status, &SessionStatus::progressReported);
    connect(emulation, &Konsole::Emulation::programStatusReported, programStatus, &ProgramStatusController::handle);
    connect(emulation, &Konsole::Emulation::promptStarted, programStatus, &ProgramStatusController::promptStarted);
    connect(emulation, &Konsole::Emulation::terminalReset, programStatus, &ProgramStatusController::terminalReset);
    connect(session, &Konsole::Session::tabStatusReceived, status, [status](const Konsole::TabStatusUpdate &update) {
        status->apply(StatusUpdate::fromTabStatus(update));
    });
    connect(session, &Konsole::Session::finished, programStatus, &ProgramStatusController::programExited);
    connect(session, &Konsole::Session::finished, status, &SessionStatus::programDidExit);
    connect(status, &SessionStatus::changed, this, [this, session] {
        Q_EMIT statusChanged(session);
    });
    connect(session, &QObject::destroyed, this, [this, session] {
        // The status owns its program status controller.
        delete _sessions.take(session).status;
    });
}

#include "moc_sessionstatusregistry.cpp"
