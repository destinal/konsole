// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "statusnotifier.h"
#include "sessionnavigation.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"

#include "session/Session.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KLocalizedString>
#include <KNotification>

#include <QPointer>
#include <QWindow>

namespace
{
// The same event for the same session is not repeated within this time.
constexpr qint64 repeatIntervalMs = 10 * 1000;
}

StatusNotifier::StatusNotifier(SessionStatusRegistry *registry, QObject *parent)
    : QObject(parent)
    , _registry(registry)
{
    connect(_registry, &SessionStatusRegistry::statusChanged, this, &StatusNotifier::statusChanged);
}

void StatusNotifier::statusChanged(Konsole::Session *session, const std::optional<QString> &previousStatusText)
{
    SessionStatus *status = _registry->status(session);
    if (status == nullptr) {
        return;
    }
    const QString text = status->statusText().value_or(QString()).toLower();
    if (text == previousStatusText.value_or(QString()).toLower()) {
        return;
    }

    QString event;
    QString fallbackText;
    if (text == QLatin1String("waiting")) {
        event = QStringLiteral("ClaudeCodeWaiting");
        fallbackText = i18n("Waiting for you");
    } else if (text == QLatin1String("done")) {
        event = QStringLiteral("ClaudeCodeFinished");
        fallbackText = i18n("Finished");
    } else if (text == QLatin1String("error")) {
        event = QStringLiteral("ClaudeCodeFinished");
        fallbackText = i18n("Failed");
    } else {
        return;
    }

    if (ClaudeCode::isSessionFocused(session)) {
        return;
    }
    const auto sent = _sent.constFind(session);
    if (sent != _sent.cend() && sent->event == event && sent->timer.elapsed() < repeatIntervalMs) {
        return;
    }
    if (!_sent.contains(session)) {
        // Once per session: a lambda cannot be a unique connection.
        connect(session, &QObject::destroyed, this, [this, session] {
            _sent.remove(session);
        });
    }
    Sent record{event, QElapsedTimer()};
    record.timer.start();
    _sent.insert(session, record);

    auto *notification = new KNotification(event, KNotification::CloseWhenWindowActivated);
    notification->setTitle(i18nc("@title a Claude Code session, named by its tab title", "Claude Code — %1", ClaudeCode::sessionTitle(session)));
    notification->setText(status->detail().value_or(fallbackText).toHtmlEscaped());
    const auto views = session->views();
    if (!views.isEmpty() && views.first()->window()->windowHandle() != nullptr) {
        notification->setWindow(views.first()->window()->windowHandle());
    }
    KNotificationAction *show = notification->addDefaultAction(i18nc("@action bring the session forward", "Show"));
    connect(show, &KNotificationAction::activated, this, [notification, session = QPointer<Konsole::Session>(session)] {
        if (session != nullptr) {
            ClaudeCode::activateSession(session, notification->xdgActivationToken());
        }
    });
    notification->sendEvent();
}

#include "moc_statusnotifier.cpp"
