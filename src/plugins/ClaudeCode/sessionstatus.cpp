// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionstatus.h"

#include <QTimer>

#include <algorithm>
#include <limits>

using Konsole::TabStatusFields;
using Konsole::TerminalProgress;

namespace
{
// cc-status's idle colors, used when a key press turns a finished turn idle.
const QColor idleIndicator(0x00, 0xd7, 0x5f);
const QColor idleStatusColor(0x88, 0x88, 0x88);

std::optional<QString> stringArgument(const QVariantMap &arguments, const char *key)
{
    const auto it = arguments.constFind(QLatin1String(key));
    if (it == arguments.cend()) {
        return std::nullopt;
    }
    return it->toString();
}

std::optional<qint64> integerArgument(const QVariantMap &arguments, const char *key, bool *valid)
{
    const auto it = arguments.constFind(QLatin1String(key));
    if (it == arguments.cend()) {
        return std::nullopt;
    }
    bool ok = false;
    const qint64 value = it->toLongLong(&ok);
    if (!ok) {
        *valid = false;
    }
    return value;
}

std::optional<bool> boolArgument(const QVariantMap &arguments, const char *key, bool *valid)
{
    const auto it = arguments.constFind(QLatin1String(key));
    if (it == arguments.cend()) {
        return std::nullopt;
    }
    if (it->typeId() == QMetaType::Bool) {
        return it->toBool();
    }
    const QString text = it->toString();
    if (text == QLatin1String("true") || text == QLatin1String("1")) {
        return true;
    }
    if (text == QLatin1String("false") || text == QLatin1String("0")) {
        return false;
    }
    *valid = false;
    return std::nullopt;
}

// A color argument: #rrggbb or #rgb. An empty one clears the field, which an
// invalid QColor stands for.
std::optional<QColor> parseColor(const QString &text)
{
    if (text.isEmpty()) {
        return QColor();
    }
    if (!text.startsWith(QLatin1Char('#')) || (text.size() != 4 && text.size() != 7)) {
        return std::nullopt;
    }
    const QColor color = QColor::fromString(text);
    return color.isValid() ? std::optional<QColor>(color) : std::nullopt;
}

// Fills one set of visible fields. The status and the fallback it expires to
// take the same fields, parsed the same way. Returns false and sets the error
// when a color does not parse.
bool populate(TabStatusFields &fields, const QVariantMap &arguments, const QString &prefix, QString *error)
{
    const auto key = [&prefix](const char *name) {
        return (prefix + QLatin1String(name)).toLatin1();
    };
    fields.status = stringArgument(arguments, key("status").constData());
    fields.detail = stringArgument(arguments, key("detail").constData());

    for (const char *name : {"text_color", "dot_color"}) {
        const auto text = stringArgument(arguments, key(name).constData());
        if (!text.has_value()) {
            continue;
        }
        const auto color = parseColor(*text);
        if (!color.has_value()) {
            *error = QStringLiteral("Invalid %1%2 (expected #rrggbb)").arg(prefix, QLatin1String(name));
            return false;
        }
        (qstrcmp(name, "text_color") == 0 ? fields.statusColor : fields.indicator) = color;
    }
    return true;
}
}

bool StatusUpdate::assertsStatus() const
{
    return fields != TabStatusFields() || expiresOnProgressEnd;
}

StatusUpdate StatusUpdate::fromTabStatus(const Konsole::TabStatusUpdate &update)
{
    StatusUpdate result;
    result.fields = update.fields;
    result.expiresOnProgressEnd = update.expiresOnProgressEnd;
    result.fallback = update.fallback;
    return result;
}

std::optional<StatusUpdate> StatusUpdate::fromArguments(const QVariantMap &arguments, QString *error)
{
    StatusUpdate update;
    if (!populate(update.fields, arguments, QString(), error)) {
        return std::nullopt;
    }

    bool valid = true;
    const auto backgroundTasks = integerArgument(arguments, "background_tasks", &valid);
    const auto delta = integerArgument(arguments, "background_tasks_delta", &valid);
    update.turnOpen = boolArgument(arguments, "turn_open", &valid);
    update.requiredTurnOpen = boolArgument(arguments, "if_turn_open", &valid);
    update.requiredBackgroundTasks = integerArgument(arguments, "if_background_tasks", &valid);
    if (!valid) {
        *error = QStringLiteral("Invalid number or boolean argument");
        return std::nullopt;
    }
    if (backgroundTasks.has_value() && delta.has_value()) {
        *error = QStringLiteral("Pass background_tasks or background_tasks_delta, not both.");
        return std::nullopt;
    }
    if (backgroundTasks.has_value()) {
        update.backgroundTasks = std::max<qint64>(0, *backgroundTasks);
    }
    update.backgroundTasksDelta = delta;

    const QString then = QStringLiteral("then_");
    const bool hasFallbackArguments = std::any_of(arguments.keyBegin(), arguments.keyEnd(), [&then](const QString &key) {
        return key.startsWith(then);
    });
    const QString expiresOn = stringArgument(arguments, "expires_on").value_or(QString());
    if (!expiresOn.isEmpty()) {
        if (expiresOn != QLatin1String("progress-end")) {
            *error = QStringLiteral("Unknown expires_on \"%1\". The only supported value is progress-end.").arg(expiresOn);
            return std::nullopt;
        }
        // With no then_ arguments the status simply goes away when it expires.
        if (hasFallbackArguments) {
            if (!populate(update.fallback, arguments, then, error)) {
                return std::nullopt;
            }
        } else {
            update.fallback = TabStatusFields::cleared();
        }
        update.expiresOnProgressEnd = true;
    } else if (hasFallbackArguments) {
        *error = QStringLiteral("The then_ arguments only mean something with expires_on, which was not given.");
        return std::nullopt;
    }
    return update;
}

SessionStatus::SessionStatus(QObject *parent, int progressEndDelayMs)
    : QObject(parent)
    , _progressEndDelayMs(progressEndDelayMs)
{
}

std::optional<QString> SessionStatus::statusText() const
{
    return _display.statusText;
}

QColor SessionStatus::indicator() const
{
    return _display.indicator;
}

QColor SessionStatus::statusColor() const
{
    return _display.statusColor;
}

std::optional<QString> SessionStatus::detail() const
{
    return _display.detail;
}

qint64 SessionStatus::backgroundTasks() const
{
    return _backgroundTasks;
}

std::optional<bool> SessionStatus::turnOpen() const
{
    return _turnOpen;
}

bool SessionStatus::hasActiveStatus() const
{
    return _display.indicator.isValid() || _display.statusText.has_value();
}

bool SessionStatus::hasArmedExpiration() const
{
    return _armedFallback.has_value();
}

bool SessionStatus::preconditionsHold(const StatusUpdate &update) const
{
    // An unknown turn flag satisfies neither value.
    if (update.requiredTurnOpen.has_value() && _turnOpen != update.requiredTurnOpen) {
        return false;
    }
    if (update.requiredBackgroundTasks.has_value() && _backgroundTasks != *update.requiredBackgroundTasks) {
        return false;
    }
    return true;
}

bool SessionStatus::apply(const StatusUpdate &update)
{
    // A dropped update must not supersede an armed expiration either: the
    // status it would have replaced is still the one last asserted.
    if (!preconditionsHold(update)) {
        return false;
    }

    const bool asserts = update.assertsStatus();
    if (asserts) {
        // Whether the expiration being replaced was already released by the
        // timer and only held back by background work. Read before the update
        // lands, since it may carry a new count.
        const bool previousWasHeld = _armedFallback.has_value() && _headStartElapsed && _generation == _operationEndGeneration && _backgroundTasks > 0;

        // A new assertion supersedes whatever the last one asked to happen
        // when it went stale; bumping the generation drops its timer.
        _armedFallback = update.expiresOnProgressEnd ? std::optional<TabStatusFields>(update.fallback) : std::nullopt;
        ++_generation;

        // Re-armed while the last expiration was held: no operation has
        // started since the one that ended, so this expiration belongs to it,
        // and the count must stay able to release it.
        if (previousWasHeld && _armedFallback.has_value()) {
            _operationEndGeneration = _generation;
        }
    }

    const auto previousText = _display.statusText;
    if (applyToState(update)) {
        Q_EMIT changed(previousText);
    }

    // A count arriving after the head start may report that the last of the
    // background work holding an expiration has finished. Only a count can
    // change that answer, and an assertion since the operation ended belongs
    // to an operation that has not ended yet.
    const bool changesCount = update.backgroundTasks.has_value() || update.backgroundTasksDelta.has_value();
    if (!asserts && changesCount && _headStartElapsed && _generation == _operationEndGeneration) {
        expireNow();
    }
    return true;
}

bool SessionStatus::applyToState(const StatusUpdate &update)
{
    const bool changed = applyFields(update.fields);

    if (update.backgroundTasks.has_value()) {
        _backgroundTasks = *update.backgroundTasks;
    } else if (update.backgroundTasksDelta.has_value()) {
        // The delta comes from a command line, so saturate instead of overflowing.
        const qint64 delta = *update.backgroundTasksDelta;
        qint64 sum = 0;
        if (__builtin_add_overflow(_backgroundTasks, delta, &sum)) {
            _backgroundTasks = delta > 0 ? std::numeric_limits<qint64>::max() : 0;
        } else {
            _backgroundTasks = std::max<qint64>(0, sum);
        }
    }
    if (update.turnOpen.has_value()) {
        _turnOpen = update.turnOpen;
    }
    // The count and the turn flag never count as a visible change.
    return changed;
}

bool SessionStatus::applyFields(const TabStatusFields &fields)
{
    const Display before = _display;
    if (fields.indicator.has_value()) {
        _display.indicator = *fields.indicator;
    }
    if (fields.status.has_value()) {
        _display.statusText = fields.status->isEmpty() ? std::nullopt : fields.status;
    }
    if (fields.statusColor.has_value()) {
        _display.statusColor = *fields.statusColor;
    }
    if (fields.detail.has_value()) {
        _display.detail = fields.detail->isEmpty() ? std::nullopt : fields.detail;
    }
    return !(_display == before);
}

void SessionStatus::progressReported(const std::optional<TerminalProgress> &progress)
{
    // A stop ends the operation, and so does an error: a program that reports
    // a failure may exit without ever reporting a stop. Paused and percentages
    // mean it is still running.
    const bool endsOperation = !progress.has_value() || progress->state == TerminalProgress::Error;
    if (!endsOperation) {
        _progressWasRunning = true;
        _headStartElapsed = false;
        // Whatever ended a moment ago evidently did not stay ended.
        ++_generation;
        return;
    }
    if (!_progressWasRunning) {
        return;
    }
    _progressWasRunning = false;
    ++_generation;
    _operationEndGeneration = _generation;
    if (!_armedFallback.has_value()) {
        return;
    }
    const int scheduled = _generation;
    QTimer::singleShot(_progressEndDelayMs, this, [this, scheduled] {
        expire(scheduled);
    });
}

void SessionStatus::expire(int scheduledGeneration)
{
    if (scheduledGeneration != _generation) {
        return;
    }
    // From here a held expiration is released the moment its count changes.
    _headStartElapsed = true;
    expireNow();
}

void SessionStatus::expireNow()
{
    // Stays armed while background work is counted: the program has not
    // finished, and the status it last asserted is still the true one.
    if (!_armedFallback.has_value() || _backgroundTasks != 0) {
        return;
    }
    const TabStatusFields fallback = *_armedFallback;
    _armedFallback.reset();
    const auto previousText = _display.statusText;
    if (applyFields(fallback)) {
        Q_EMIT changed(previousText);
    }
}

void SessionStatus::userDidPressKey()
{
    const QString text = _display.statusText.value_or(QString()).toLower();
    if (text != QLatin1String("done") && text != QLatin1String("error")) {
        return;
    }
    StatusUpdate idle;
    idle.fields.status = QStringLiteral("idle");
    idle.fields.indicator = idleIndicator;
    idle.fields.statusColor = idleStatusColor;
    apply(idle);
}

bool SessionStatus::clearStatus()
{
    // An expiration can be armed without a visible field, and must not
    // outlive the clear.
    if (!hasActiveStatus() && !_armedFallback.has_value()) {
        return false;
    }
    _armedFallback.reset();
    ++_generation;
    const auto previousText = _display.statusText;
    _display = Display();
    _backgroundTasks = 0;
    _turnOpen.reset();
    Q_EMIT changed(previousText);
    return true;
}

void SessionStatus::programDidExit()
{
    const bool wasActive = hasActiveStatus();
    const auto previousText = _display.statusText;
    _display = Display();
    _backgroundTasks = 0;
    _turnOpen.reset();
    _armedFallback.reset();
    ++_generation;
    // A restarted program starts from having reported nothing.
    _progressWasRunning = false;
    _headStartElapsed = false;
    _operationEndGeneration = 0;
    if (wasActive) {
        Q_EMIT changed(previousText);
    }
}

QJsonObject SessionStatus::toJson() const
{
    const auto text = [](const std::optional<QString> &value) {
        return value.has_value() ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
    };
    const auto color = [](const QColor &value) {
        return value.isValid() ? QJsonValue(value.name()) : QJsonValue(QJsonValue::Null);
    };
    return {
        {QStringLiteral("status"), text(_display.statusText)},
        {QStringLiteral("text_color"), color(_display.statusColor)},
        {QStringLiteral("dot_color"), color(_display.indicator)},
        {QStringLiteral("detail"), text(_display.detail)},
        {QStringLiteral("background_tasks"), _backgroundTasks},
        {QStringLiteral("turn_open"), _turnOpen.has_value() ? QJsonValue(*_turnOpen) : QJsonValue(QJsonValue::Null)},
    };
}

#include "moc_sessionstatus.cpp"
