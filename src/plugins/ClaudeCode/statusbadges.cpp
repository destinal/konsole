// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "statusbadges.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"

#include "session/Session.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace
{
KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("ClaudeCode"));
}
}

StatusBadges::StatusBadges(SessionStatusRegistry *registry, QObject *parent)
    : QObject(parent)
    , _registry(registry)
    , _enabled(settings().readEntry("ShowStatusBadges", false))
{
    connect(_registry, &SessionStatusRegistry::statusChanged, this, &StatusBadges::update);
    const auto sessions = _registry->sessions();
    for (Konsole::Session *session : sessions) {
        update(session);
    }
}

bool StatusBadges::isEnabled() const
{
    return _enabled;
}

void StatusBadges::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }
    _enabled = enabled;
    KConfigGroup group = settings();
    group.writeEntry("ShowStatusBadges", enabled);
    group.sync();

    const auto sessions = _registry->sessions();
    for (Konsole::Session *session : sessions) {
        update(session);
    }
    Q_EMIT enabledChanged(enabled);
}

void StatusBadges::update(Konsole::Session *session)
{
    SessionStatus *status = _registry->status(session);
    const bool show = _enabled && status != nullptr && status->statusText().has_value();
    const bool shown = _saved.contains(session);

    if (!show) {
        if (shown) {
            setBadge(session, _saved.take(session));
        }
        return;
    }

    if (!shown) {
        _saved.insert(session, badgeOf(session));
        connect(
            session,
            &QObject::destroyed,
            this,
            [this, session] {
                _saved.remove(session);
            },
            Qt::UniqueConnection);
    }
    Badge badge;
    badge.enabled = true;
    badge.text = *status->statusText();
    badge.color = status->indicator().isValid() ? status->indicator() : status->statusColor();
    badge.textOnly = false;
    setBadge(session, badge);
}

StatusBadges::Badge StatusBadges::badgeOf(Konsole::Session *session)
{
    return {session->badgeEnabled(), session->badgeText(), session->badgeColor(), session->badgeTextOnly()};
}

void StatusBadges::setBadge(Konsole::Session *session, const Badge &badge)
{
    session->setBadgeText(badge.text);
    session->setBadgeColor(badge.color);
    session->setBadgeTextOnly(badge.textOnly);
    session->setBadgeEnabled(badge.enabled);
}

#include "moc_statusbadges.cpp"
