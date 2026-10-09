// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudehooks.h"
#include "workgroupmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace
{
// The events konsole-claude-setup hooks.
const QStringList events = {QStringLiteral("UserPromptSubmit"),
                            QStringLiteral("Stop"),
                            QStringLiteral("StopFailure"),
                            QStringLiteral("Notification"),
                            QStringLiteral("PermissionRequest"),
                            QStringLiteral("SessionEnd"),
                            QStringLiteral("PreToolUse"),
                            QStringLiteral("PostToolUse"),
                            QStringLiteral("SessionStart"),
                            QStringLiteral("SubagentStop")};

QString setupProgram()
{
    // A build tree keeps it with the other tools; an installation, on PATH.
    const QString beside = WorkgroupManager::toolsDirectory() + QLatin1String("/konsole-claude-setup");
    if (QFileInfo(beside).isExecutable()) {
        return beside;
    }
    return QStandardPaths::findExecutable(QStringLiteral("konsole-claude-setup"));
}
}

ClaudeHooks::ClaudeHooks(QObject *parent)
    : QObject(parent)
    , _watcher(new QFileSystemWatcher(this))
{
    // The directory, not the file: Claude Code and the setup replace the file
    // by renaming a new one over it, which a watch on the file would lose.
    QDir().mkpath(QFileInfo(settingsPath()).path());
    _watcher->addPath(QFileInfo(settingsPath()).path());
    connect(_watcher, &QFileSystemWatcher::directoryChanged, this, &ClaudeHooks::check);
    check();
}

QString ClaudeHooks::settingsPath() const
{
    QString directory = qEnvironmentVariable("CLAUDE_CONFIG_DIR");
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QLatin1String("/.claude");
    }
    return directory + QLatin1String("/settings.json");
}

bool ClaudeHooks::isInstalled() const
{
    return _installed;
}

void ClaudeHooks::check()
{
    QFile file(settingsPath());
    bool installed = false;
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject hooks = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("hooks")).toObject();
        installed = std::all_of(events.begin(), events.end(), [&hooks](const QString &event) {
            const QJsonArray groups = hooks.value(event).toArray();
            return std::any_of(groups.begin(), groups.end(), [](const QJsonValue &group) {
                const QJsonArray entries = group.toObject().value(QStringLiteral("hooks")).toArray();
                return std::any_of(entries.begin(), entries.end(), [](const QJsonValue &entry) {
                    return entry.toObject().value(QStringLiteral("command")).toString().contains(QLatin1String("claude-code-hook"));
                });
            });
        });
    }
    if (installed != _installed) {
        _installed = installed;
        Q_EMIT installedChanged(installed);
    }
}

bool ClaudeHooks::run(const QString &command, QString *output)
{
    const QString program = setupProgram();
    if (program.isEmpty()) {
        *output = QStringLiteral("konsole-claude-setup is not installed.");
        return false;
    }
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, {command});
    const bool finished = process.waitForFinished(10000);
    *output = QString::fromUtf8(process.readAll()).trimmed();
    check();
    return finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

#include "moc_claudehooks.cpp"
