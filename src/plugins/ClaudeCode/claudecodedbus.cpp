// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudecodedbus.h"
#include "clippingstore.h"
#include "sessionstatus.h"
#include "sessionstatusregistry.h"

#include "session/Session.h"

#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ClaudeCodeDBus::ClaudeCodeDBus(SessionStatusRegistry *registry, ClippingStore *clippings, QObject *parent)
    : QObject(parent)
    , _registry(registry)
    , _clippings(clippings)
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
    const auto sessions = _registry->sessions();
    for (const Konsole::Session *session : sessions) {
        paths.append(QStringLiteral("/Sessions/%1").arg(session->sessionId()));
    }
    paths.sort();
    return paths;
}

QString ClaudeCodeDBus::setStatus(const QString &session, const QVariantMap &arguments)
{
    SessionStatus *status = statusOrError(session);
    if (status == nullptr) {
        return {};
    }
    QString error;
    const auto update = StatusUpdate::fromArguments(arguments, &error);
    if (!update.has_value()) {
        sendErrorReply(QStringLiteral("org.kde.konsole.ClaudeCode.InvalidArgument"), error);
        return {};
    }
    status->apply(*update);
    return QString::fromUtf8(QJsonDocument(status->toJson()).toJson(QJsonDocument::Compact));
}

QString ClaudeCodeDBus::getStatus(const QString &session)
{
    SessionStatus *status = statusOrError(session);
    if (status == nullptr) {
        return {};
    }
    return QString::fromUtf8(QJsonDocument(status->toJson()).toJson(QJsonDocument::Compact));
}

qlonglong ClaudeCodeDBus::getBackgroundTaskCount(const QString &session)
{
    SessionStatus *status = statusOrError(session);
    return status != nullptr ? status->backgroundTasks() : 0;
}

void ClaudeCodeDBus::addClipping(const QString &session, const QString &type, const QString &title, const QString &detail)
{
    if (statusOrError(session) != nullptr) {
        _clippings->add(_registry->session(session), Clipping{type, title, detail, QDateTime::currentDateTime()});
    }
}

void ClaudeCodeDBus::archiveClippings(const QString &session)
{
    if (statusOrError(session) != nullptr) {
        _clippings->archive(_registry->session(session));
    }
}

QString ClaudeCodeDBus::clippings(const QString &session)
{
    if (statusOrError(session) == nullptr) {
        return {};
    }
    QJsonArray array;
    const auto live = _clippings->live(_registry->session(session));
    for (const Clipping &clipping : live) {
        array.append(
            QJsonObject{{QStringLiteral("type"), clipping.type}, {QStringLiteral("title"), clipping.title}, {QStringLiteral("detail"), clipping.detail}});
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

SessionStatus *ClaudeCodeDBus::statusOrError(const QString &session)
{
    SessionStatus *status = _registry->status(_registry->session(session));
    if (status == nullptr) {
        sendErrorReply(QStringLiteral("org.kde.konsole.ClaudeCode.NoSuchSession"), QStringLiteral("No such session: %1").arg(session));
    }
    return status;
}

#include "moc_claudecodedbus.cpp"
