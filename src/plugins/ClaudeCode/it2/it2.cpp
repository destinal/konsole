// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// A stand-in for iTerm2's it2 command, covering the calls cc-status makes:
//
//   it2 set-status --session=<id> [--quiet-if-unresolved] [--json] [--status=...] ...
//   it2 session get-status --session=<id> [--quiet-if-unresolved]
//   it2 session get-background-tasks --session=<id> [--quiet-if-unresolved]
//   it2 session add-clipping --session=<id> TYPE TITLE DETAIL
//   it2 session archive-clippings --session=<id>
//
// Each becomes a call to the Claude Code plugin's D-Bus object in Konsole.
// The session id is what cc-status takes from $TERM_SESSION_ID, which the
// Konsole hook wrapper sets to "konsole:<D-Bus service><session path>", for
// example "konsole:org.kde.konsole-1234/Sessions/3".
//
// Options take "--name=value" or "--name value". An empty value always comes
// as two arguments. Exit status: 0 on success, 1 when the call failed, 2 for
// a usage error.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QStringList>
#include <QTextStream>
#include <QVariantMap>

#include <cstdio>

namespace
{
enum class Kind {
    Text,
    Integer,
    Boolean,
};

// The set-status options and how each is sent to the plugin. The D-Bus
// argument names are iTerm2's set_session_status ones.
struct StatusOption {
    const char *name;
    const char *argument;
    Kind kind;
};

const StatusOption statusOptions[] = {
    {"status", "status", Kind::Text},
    {"dot-color", "dot_color", Kind::Text},
    {"text-color", "text_color", Kind::Text},
    {"detail", "detail", Kind::Text},
    {"background-tasks", "background_tasks", Kind::Integer},
    {"background-tasks-delta", "background_tasks_delta", Kind::Integer},
    {"turn-open", "turn_open", Kind::Boolean},
    {"if-turn-open", "if_turn_open", Kind::Boolean},
    {"if-background-tasks", "if_background_tasks", Kind::Integer},
    {"expires-on", "expires_on", Kind::Text},
    {"then-status", "then_status", Kind::Text},
    {"then-dot-color", "then_dot_color", Kind::Text},
    {"then-text-color", "then_text_color", Kind::Text},
    {"then-detail", "then_detail", Kind::Text},
};

const QStringList addressOptions = {QStringLiteral("session"), QStringLiteral("tmux"), QStringLiteral("tmux-pane")};
const QStringList flagOptions = {QStringLiteral("json"), QStringLiteral("quiet-if-unresolved")};

struct Invocation {
    QString command;
    QHash<QString, QString> options;
    QSet<QString> flags;
    QStringList positional;
};

const QStringList sessionCommands = {QStringLiteral("get-status"),
                                     QStringLiteral("get-background-tasks"),
                                     QStringLiteral("add-clipping"),
                                     QStringLiteral("archive-clippings")};

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

bool isValueOption(const QString &name)
{
    if (addressOptions.contains(name)) {
        return true;
    }
    for (const auto &option : statusOptions) {
        if (name == QLatin1String(option.name)) {
            return true;
        }
    }
    return false;
}

// Parses the command line, or returns false after printing a usage error.
bool parse(const QStringList &arguments, Invocation *invocation)
{
    int i = 0;
    if (arguments.value(0) == QLatin1String("set-status")) {
        invocation->command = QStringLiteral("set-status");
        i = 1;
    } else if (arguments.value(0) == QLatin1String("session") && sessionCommands.contains(arguments.value(1))) {
        invocation->command = arguments.value(1);
        i = 2;
    } else {
        err() << "it2: unsupported command: " << arguments.join(QLatin1Char(' ')) << Qt::endl;
        return false;
    }

    for (; i < arguments.size(); i++) {
        const QString &argument = arguments.at(i);
        if (!argument.startsWith(QLatin1String("--"))) {
            invocation->positional.append(argument);
            continue;
        }
        const qsizetype separator = argument.indexOf(QLatin1Char('='));
        const QString name = argument.mid(2, separator < 0 ? -1 : separator - 2);
        if (flagOptions.contains(name) && separator < 0) {
            invocation->flags.insert(name);
        } else if (!isValueOption(name)) {
            err() << "it2: unknown option: --" << name << Qt::endl;
            return false;
        } else if (separator >= 0) {
            invocation->options.insert(name, argument.mid(separator + 1));
        } else if (i + 1 < arguments.size()) {
            invocation->options.insert(name, arguments.at(++i));
        } else {
            err() << "it2: missing value for --" << name << Qt::endl;
            return false;
        }
    }
    const int expected = invocation->command == QLatin1String("add-clipping") ? 3 : 0;
    if (invocation->positional.size() != expected) {
        if (expected == 0) {
            err() << "it2: unexpected argument: " << invocation->positional.value(0) << Qt::endl;
        } else {
            err() << "it2: add-clipping takes TYPE TITLE DETAIL" << Qt::endl;
        }
        return false;
    }
    return true;
}

// Splits a session id into the Konsole D-Bus service and session path.
bool resolve(const Invocation &invocation, QString *service, QString *session)
{
    // A tmux pane is not something Konsole can map to a session yet.
    if (invocation.options.contains(QStringLiteral("tmux"))) {
        return false;
    }
    QString id = invocation.options.value(QStringLiteral("session"));
    // Either what cc-status passes (the part of $TERM_SESSION_ID after its
    // first colon) or all of $TERM_SESSION_ID, as the review prompt passes it.
    const QLatin1String prefix("konsole:");
    if (id.startsWith(prefix)) {
        id.remove(0, prefix.size());
    }
    const qsizetype path = id.indexOf(QLatin1String("/Sessions/"));
    if (path <= 0) {
        return false;
    }
    *service = id.left(path);
    *session = id.mid(path);
    return true;
}

// Builds the set-status arguments for the plugin, or returns false after
// printing what is wrong.
bool statusArguments(const Invocation &invocation, QVariantMap *arguments)
{
    for (const auto &option : statusOptions) {
        const auto it = invocation.options.constFind(QLatin1String(option.name));
        if (it == invocation.options.cend()) {
            continue;
        }
        bool ok = true;
        QVariant value;
        switch (option.kind) {
        case Kind::Text:
            value = *it;
            break;
        case Kind::Integer:
            value = it->toLongLong(&ok);
            break;
        case Kind::Boolean:
            ok = *it == QLatin1String("true") || *it == QLatin1String("false");
            value = *it == QLatin1String("true");
            break;
        }
        if (!ok) {
            err() << "it2: invalid value for --" << option.name << ": " << *it << Qt::endl;
            return false;
        }
        arguments->insert(QLatin1String(option.argument), value);
    }
    return true;
}

// Whether a failed call means the session could not be found, as opposed to
// the call itself being wrong. Konsole not running, the plugin not loaded and
// an unknown session all count.
bool isUnresolved(const QDBusMessage &reply)
{
    const QString name = reply.errorName();
    return name == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown") || name == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")
        || name == QLatin1String("org.freedesktop.DBus.Error.UnknownObject") || name == QLatin1String("org.freedesktop.DBus.Error.UnknownMethod")
        || name == QLatin1String("org.freedesktop.DBus.Error.UnknownInterface") || name == QLatin1String("org.kde.konsole.ClaudeCode.NoSuchSession");
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    Invocation invocation;
    if (!parse(arguments, &invocation)) {
        return 2;
    }
    const bool quiet = invocation.flags.contains(QStringLiteral("quiet-if-unresolved"));

    QString service;
    QString session;
    if (!resolve(invocation, &service, &session)) {
        if (quiet) {
            return 0;
        }
        err() << "it2: cannot find a Konsole session for this pane" << Qt::endl;
        return 1;
    }

    QDBusMessage call;
    const QString path = QStringLiteral("/ClaudeCode");
    const QString interface = QStringLiteral("org.kde.konsole.ClaudeCode");
    if (invocation.command == QLatin1String("set-status")) {
        QVariantMap statusArgs;
        if (!statusArguments(invocation, &statusArgs)) {
            return 2;
        }
        call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("setStatus"));
        call << session << statusArgs;
    } else if (invocation.command == QLatin1String("add-clipping")) {
        call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("addClipping"));
        call << session << invocation.positional.at(0) << invocation.positional.at(1) << invocation.positional.at(2);
    } else if (invocation.command == QLatin1String("archive-clippings")) {
        call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("archiveClippings"));
        call << session;
    } else if (invocation.command == QLatin1String("get-status")) {
        call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("getStatus"));
        call << session;
    } else {
        call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("getBackgroundTaskCount"));
        call << session;
    }

    // A status update is cosmetic: never hold up the hook for long.
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (quiet && isUnresolved(reply)) {
            return 0;
        }
        err() << "it2: " << reply.errorMessage() << Qt::endl;
        return 1;
    }

    const QVariant result = reply.arguments().value(0);
    if (invocation.command == QLatin1String("add-clipping")) {
        out() << "Clipping added." << Qt::endl;
    } else if (invocation.command == QLatin1String("archive-clippings")) {
        out() << "Clippings archived." << Qt::endl;
    } else if (invocation.command == QLatin1String("set-status") && !invocation.flags.contains(QStringLiteral("json"))) {
        out() << "Session status updated." << Qt::endl;
    } else {
        out() << result.toString() << Qt::endl;
    }
    return 0;
}
