// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clippingstore.h"

#include "session/Session.h"

#include <algorithm>

QString Clipping::formatted() const
{
    if (title.trimmed().isEmpty()) {
        return detail;
    }
    return QStringLiteral("**%1**\n%2").arg(title, detail);
}

ClippingStore::ClippingStore(QObject *parent)
    : QObject(parent)
{
}

QList<Clipping> ClippingStore::live(Konsole::Session *session) const
{
    return _clippings.value(session).live;
}

QList<Clipping> ClippingStore::archived(Konsole::Session *session) const
{
    return _clippings.value(session).archived;
}

void ClippingStore::watch(Konsole::Session *session)
{
    // Once per session: a lambda cannot be a unique connection.
    if (_clippings.contains(session)) {
        return;
    }
    connect(session, &QObject::destroyed, this, [this, session] {
        _clippings.remove(session);
    });
}

void ClippingStore::add(Konsole::Session *session, const Clipping &clipping)
{
    watch(session);
    _clippings[session].live.append(clipping);
    Q_EMIT changed(session);
    Q_EMIT added(session);
}

void ClippingStore::archive(Konsole::Session *session)
{
    auto it = _clippings.find(session);
    if (it == _clippings.end() || it->live.isEmpty()) {
        return;
    }
    it->archived.append(it->live);
    it->live.clear();
    Q_EMIT changed(session);
}

void ClippingStore::archive(Konsole::Session *session, QList<int> indexes)
{
    auto it = _clippings.find(session);
    if (it == _clippings.end()) {
        return;
    }
    // From the back, so earlier indexes stay valid.
    std::sort(indexes.begin(), indexes.end(), std::greater<int>());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    bool archived = false;
    for (int index : std::as_const(indexes)) {
        if (index >= 0 && index < it->live.size()) {
            it->archived.append(it->live.takeAt(index));
            archived = true;
        }
    }
    if (archived) {
        Q_EMIT changed(session);
    }
}

QString ClippingStore::formatted(const QList<Clipping> &clippings)
{
    QStringList parts;
    for (const Clipping &clipping : clippings) {
        parts.append(clipping.formatted());
    }
    return parts.join(QLatin1String("\n--\n"));
}

#include "moc_clippingstore.cpp"
