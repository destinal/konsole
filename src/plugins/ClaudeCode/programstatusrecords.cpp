// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "programstatusrecords.h"
#include "sessionstatus.h"

#include <KLocalizedString>

#include <QColor>

using Konsole::ProgramStatusReport;

namespace
{
using State = ProgramStatusReport::State;

const QSet<State> transientStates = {ProgramStatusReport::Working, ProgramStatusReport::Blocked, ProgramStatusReport::Idle};
const QSet<State> finishedStates = {ProgramStatusReport::Done, ProgramStatusReport::Error};

int urgency(State state)
{
    switch (state) {
    case ProgramStatusReport::Blocked:
        return 5;
    case ProgramStatusReport::Error:
        return 4;
    case ProgramStatusReport::Done:
        return 3;
    case ProgramStatusReport::Working:
        return 2;
    case ProgramStatusReport::Idle:
        return 1;
    case ProgramStatusReport::Clear:
        break;
    }
    return 0;
}

QString key(const QStringList &id)
{
    return id.join(QLatin1Char('/'));
}

std::optional<QString> kindDescription(const std::optional<ProgramStatusReport::Kind> &kind)
{
    if (!kind.has_value()) {
        return std::nullopt;
    }
    switch (*kind) {
    case ProgramStatusReport::Permission:
        return i18n("Waiting for approval");
    case ProgramStatusReport::Question:
        return i18n("Waiting for an answer");
    case ProgramStatusReport::Auth:
        return i18n("Waiting for login");
    }
    return std::nullopt;
}
}

ProgramStatusRecords::ProgramStatusRecords(int capacity)
    : _capacity(capacity)
{
}

bool ProgramStatusRecords::isEmpty() const
{
    return _records.isEmpty();
}

int ProgramStatusRecords::size() const
{
    return _records.size();
}

std::optional<ProgramStatusRecords::Record> ProgramStatusRecords::record(const QStringList &id) const
{
    const auto it = _records.constFind(key(id));
    return it == _records.cend() ? std::nullopt : std::optional(*it);
}

void ProgramStatusRecords::apply(const ProgramStatusReport &report)
{
    const QString recordKey = key(report.id);
    if (report.state == ProgramStatusReport::Clear) {
        if (report.id.isEmpty()) {
            _records.clear();
        } else {
            const QString prefix = recordKey + QLatin1Char('/');
            _records.removeIf([&](const auto &it) {
                return it.key() == recordKey || it.key().startsWith(prefix);
            });
        }
        return;
    }
    // A new record over capacity evicts the least recently written one.
    if (!_records.contains(recordKey) && _records.size() >= _capacity) {
        auto oldest = _records.begin();
        for (auto it = _records.begin(); it != _records.end(); ++it) {
            if (it->serial < oldest->serial) {
                oldest = it;
            }
        }
        _records.erase(oldest);
    }
    _records.insert(recordKey, Record{report, ++_serial});
}

bool ProgramStatusRecords::remove(const QSet<State> &states)
{
    return _records.removeIf([&states](const auto &it) {
        return states.contains(it->report.state);
    }) > 0;
}

void ProgramStatusRecords::removeAll()
{
    _records.clear();
}

bool ProgramStatusRecords::markSeen(const QSet<State> &states)
{
    bool changed = false;
    for (auto &record : _records) {
        if (states.contains(record.report.state)) {
            // The program did not write this, so the record keeps its serial.
            record.report.state = ProgramStatusReport::Idle;
            changed = true;
        }
    }
    return changed;
}

std::optional<ProgramStatusRecords::Record> ProgramStatusRecords::mostUrgent() const
{
    const Record *best = nullptr;
    for (const Record &record : _records) {
        if (best == nullptr) {
            best = &record;
            continue;
        }
        const int candidate = urgency(record.report.state);
        const int current = urgency(best->report.state);
        if (candidate > current || (candidate == current && record.serial > best->serial)) {
            best = &record;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    Record result = *best;
    if (!result.report.app.has_value()) {
        result.report.app = inheritedApp(result.report.id);
    }
    return result;
}

std::optional<QString> ProgramStatusRecords::inheritedApp(QStringList id) const
{
    while (!id.isEmpty()) {
        id.removeLast();
        const auto it = _records.constFind(key(id));
        if (it != _records.cend() && it->report.app.has_value()) {
            return it->report.app;
        }
    }
    return std::nullopt;
}

ProgramStatusController::ProgramStatusController(SessionStatus *status, QObject *parent)
    : QObject(parent)
    , _status(status)
{
    connect(_status, &SessionStatus::asserted, this, [this] {
        if (!_applying) {
            otherWriterDidSetStatus();
        }
    });
}

const ProgramStatusRecords &ProgramStatusController::records() const
{
    return _records;
}

void ProgramStatusController::handle(const ProgramStatusReport &report)
{
    _records.apply(report);
    refresh(true);
}

void ProgramStatusController::promptStarted()
{
    if (_records.remove(transientStates) && _isShowingStatus) {
        refresh(true);
    }
}

void ProgramStatusController::terminalReset()
{
    _records.removeAll();
    refresh(_isShowingStatus);
}

void ProgramStatusController::programExited()
{
    _records.removeAll();
    _isShowingStatus = false;
}

void ProgramStatusController::userDidPressKey()
{
    if (_records.markSeen(finishedStates)) {
        refresh(_isShowingStatus);
    }
}

void ProgramStatusController::otherWriterDidSetStatus()
{
    _isShowingStatus = false;
}

void ProgramStatusController::refresh(bool showingStatus)
{
    if (!showingStatus) {
        return;
    }
    const auto record = _records.mostUrgent();
    StatusUpdate update;
    if (record.has_value()) {
        update = statusUpdate(*record);
    } else if (_isShowingStatus) {
        // The records put the status up and none are left.
        update.fields = Konsole::TabStatusFields::cleared();
    } else {
        return;
    }
    _applying = true;
    _status->apply(update);
    _applying = false;
    _isShowingStatus = record.has_value();
}

StatusUpdate ProgramStatusController::statusUpdate(const ProgramStatusRecords::Record &record)
{
    // The same words and colors cc-status writes, so a program using the
    // protocol looks like Claude Code with its hooks installed.
    struct Look {
        const char *status;
        QRgb indicator;
        QRgb text;
    };
    Look look = {"error", 0xff3b30, 0xff3b30};
    switch (record.report.state) {
    case ProgramStatusReport::Idle:
        look = {"idle", 0x00d75f, 0x888888};
        break;
    case ProgramStatusReport::Working:
        look = {"working", 0xff9500, 0xff9500};
        break;
    case ProgramStatusReport::Blocked:
        look = {"waiting", 0x5f87ff, 0x5f87ff};
        break;
    case ProgramStatusReport::Done:
        look = {"done", 0x00d75f, 0x00d75f};
        break;
    case ProgramStatusReport::Error:
    case ProgramStatusReport::Clear:
        break;
    }

    StatusUpdate update;
    update.fields.status = QLatin1String(look.status);
    update.fields.indicator = QColor::fromRgb(look.indicator);
    update.fields.statusColor = QColor::fromRgb(look.text);

    const auto &report = record.report;
    std::optional<QString> message = report.msg.has_value() ? std::optional(sanitized(*report.msg)) : kindDescription(report.kind);
    // The root record's label is normally the window title already, so only
    // a child record's title adds anything.
    const QString title = report.id.isEmpty() ? QString() : sanitized(report.title.value_or(QString()));
    if (!title.isEmpty()) {
        message = message.has_value() && !message->isEmpty() ? i18nc("@info a task's name, then what it is doing", "%1: %2", title, *message) : title;
    }
    // Every report replaces its record, so nothing from an earlier one may linger.
    update.fields.detail = message.value_or(QString());
    return update;
}

QString ProgramStatusController::sanitized(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (const char32_t ucs4 : text.toUcs4()) {
        const QChar::Category category = QChar::category(ucs4);
        // Keep the joiners that build emoji and shape scripts, and the tag
        // characters in subdivision flags.
        const bool keptFormat = ucs4 == 0x200C || ucs4 == 0x200D || (ucs4 >= 0xE0020 && ucs4 <= 0xE007F);
        if ((category == QChar::Other_Format && !keptFormat) || category == QChar::Separator_Line || category == QChar::Separator_Paragraph) {
            continue;
        }
        result.append(QString::fromUcs4(&ucs4, 1));
    }
    return result;
}

#include "moc_programstatusrecords.cpp"
