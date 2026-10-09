// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudesessionsmodel.h"
#include "sessionnavigation.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"

#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <QTimer>

#include <algorithm>

ClaudeSessionsModel::ClaudeSessionsModel(SessionStatusRegistry *registry, QObject *parent)
    : QAbstractListModel(parent)
    , _registry(registry)
{
    connect(_registry, &SessionStatusRegistry::statusChanged, this, [this](Konsole::Session *session) {
        _changed.insert(session, ++_clock);
        watch(session);
        scheduleRefresh();
    });
    refresh();
}

int ClaudeSessionsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _rows.size();
}

QVariant ClaudeSessionsModel::data(const QModelIndex &index, int role) const
{
    Konsole::Session *session = this->session(index);
    SessionStatus *status = session != nullptr ? _registry->status(session) : nullptr;
    if (status == nullptr) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole:
        return ClaudeCode::sessionTitle(session);
    case Qt::ToolTipRole: {
        QString tip = ClaudeCode::sessionTitle(session) + QLatin1String(" — ") + status->statusText().value_or(QString());
        if (status->shownDetail().has_value()) {
            tip += QLatin1Char('\n') + *status->shownDetail();
        }
        return tip;
    }
    case StatusRole:
        return status->statusText().value_or(QString());
    case StatusColorRole:
        return status->statusColor();
    case IndicatorRole:
        return status->indicator();
    case DetailRole:
        return status->shownDetail().value_or(QString());
    case SessionRole:
        return QVariant::fromValue<QObject *>(session);
    }
    return {};
}

Konsole::Session *ClaudeSessionsModel::session(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= _rows.size()) {
        return nullptr;
    }
    return _rows.at(index.row());
}

int ClaudeSessionsModel::priority(const QString &status)
{
    const QString lower = status.toLower();
    if (lower == QLatin1String("waiting")) {
        return 0;
    }
    if (lower == QLatin1String("error")) {
        return 1;
    }
    if (lower == QLatin1String("done")) {
        return 2;
    }
    if (lower == QLatin1String("working")) {
        return 3;
    }
    if (lower == QLatin1String("idle")) {
        return 4;
    }
    return 5;
}

void ClaudeSessionsModel::watch(Konsole::Session *session)
{
    // Titles change outside the status: follow the session and its views.
    connect(session, &Konsole::Session::sessionAttributeChanged, this, &ClaudeSessionsModel::scheduleRefresh, Qt::UniqueConnection);
    connect(session, &QObject::destroyed, this, &ClaudeSessionsModel::scheduleRefresh, Qt::UniqueConnection);
    const auto views = session->views();
    for (Konsole::TerminalDisplay *view : views) {
        if (view->sessionController() != nullptr) {
            connect(view->sessionController(), &Konsole::ViewProperties::titleChanged, this, &ClaudeSessionsModel::scheduleRefresh, Qt::UniqueConnection);
        }
    }
}

void ClaudeSessionsModel::scheduleRefresh()
{
    // Coalesce: one status change can bring several signals.
    if (_refreshScheduled) {
        return;
    }
    _refreshScheduled = true;
    QTimer::singleShot(0, this, &ClaudeSessionsModel::refresh);
}

void ClaudeSessionsModel::refresh()
{
    _refreshScheduled = false;

    QList<Konsole::Session *> sessions;
    const auto all = _registry->sessions();
    // Forget sessions that are gone.
    _changed.removeIf([&all](const auto &it) {
        return !all.contains(it.key());
    });
    for (Konsole::Session *session : all) {
        SessionStatus *status = _registry->status(session);
        if (status != nullptr && status->hasActiveStatus()) {
            sessions.append(session);
        }
    }
    std::sort(sessions.begin(), sessions.end(), [this](Konsole::Session *a, Konsole::Session *b) {
        const int pa = priority(_registry->status(a)->statusText().value_or(QString()));
        const int pb = priority(_registry->status(b)->statusText().value_or(QString()));
        if (pa != pb) {
            return pa < pb;
        }
        const quint64 ca = _changed.value(a);
        const quint64 cb = _changed.value(b);
        if (ca != cb) {
            return ca > cb;
        }
        return a->sessionId() < b->sessionId();
    });

    QList<Konsole::Session *> current;
    for (const auto &row : std::as_const(_rows)) {
        current.append(row.data());
    }
    if (current == sessions) {
        if (!_rows.isEmpty()) {
            Q_EMIT dataChanged(index(0), index(_rows.size() - 1));
        }
        return;
    }
    beginResetModel();
    _rows.clear();
    for (Konsole::Session *session : std::as_const(sessions)) {
        _rows.append(session);
    }
    endResetModel();
}

#include "moc_claudesessionsmodel.cpp"
