// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDESESSIONSMODEL_H
#define CLAUDESESSIONSMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QPointer>

namespace Konsole
{
class Session;
}
class SessionStatusRegistry;

/**
 * Every session that shows a status, in every Konsole window, the ones that
 * need the user first: waiting, error, done, working, idle, then anything
 * else, the most recently changed first among equals.
 */
class ClaudeSessionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        StatusRole = Qt::UserRole + 1,
        StatusColorRole,
        IndicatorRole,
        DetailRole,
        SessionRole,
    };

    explicit ClaudeSessionsModel(SessionStatusRegistry *registry, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Konsole::Session *session(const QModelIndex &index) const;

    /** Sort rank of a status: lower needs the user more. */
    static int priority(const QString &status);

private:
    void scheduleRefresh();
    void refresh();
    void watch(Konsole::Session *session);

    SessionStatusRegistry *_registry;
    QList<QPointer<Konsole::Session>> _rows;
    // When each session's status last changed, for ordering equals.
    QHash<Konsole::Session *, quint64> _changed;
    quint64 _clock = 0;
    bool _refreshScheduled = false;
};

#endif // CLAUDESESSIONSMODEL_H
