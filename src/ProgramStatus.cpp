/*
    SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProgramStatus.h"

#include <QByteArray>
#include <QHash>

#include <algorithm>

using namespace Konsole;

namespace
{
// Limits from the spec. A report that breaks one is discarded whole.
constexpr int maxSequenceBytes = 4096;
// ESC ] 7501 ; ... BEL is the shortest framing around the body.
constexpr int minimumFramingBytes = 8;
constexpr int maxKeyBytes = 16;
constexpr int maxMsgEncodedBytes = 2732;
constexpr int maxMsgDecodedBytes = 2048;
constexpr int maxTitleEncodedBytes = 256;
constexpr int maxTitleDecodedBytes = 192;
constexpr int maxAppBytes = 32;
constexpr int maxIdBytes = 128;
constexpr int maxIdSegmentBytes = 32;
constexpr int maxIdDepth = 8;

bool isAlphanumeric(QChar ch)
{
    return (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) || (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
        || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'));
}

bool isKeyCharacter(QChar ch)
{
    return ch >= QLatin1Char('a') && ch <= QLatin1Char('z');
}

// value := [A-Za-z0-9_.,+/=-]*
bool isValueCharacter(QChar ch)
{
    return isAlphanumeric(ch) || QStringView(u"_.,+/=-").contains(ch);
}

// segment := [A-Za-z0-9_.+-]{1,32}, which is also the grammar for app.
bool isIdSegment(QStringView segment)
{
    return !segment.isEmpty() && segment.size() <= maxIdSegmentBytes && std::all_of(segment.begin(), segment.end(), [](QChar ch) {
        return isAlphanumeric(ch) || QStringView(u"_.+-").contains(ch);
    });
}

bool withinLimits(QStringView key, QStringView value)
{
    const qsizetype length = value.size();
    if (key == u"msg") {
        return length <= maxMsgEncodedBytes;
    }
    if (key == u"title") {
        return length <= maxTitleEncodedBytes;
    }
    if (key == u"app") {
        return length <= maxAppBytes;
    }
    if (key == u"id") {
        return length <= maxIdBytes && value.count(QLatin1Char('/')) + 1 <= maxIdDepth;
    }
    return true;
}

enum class Decoded {
    Absent,
    Invalid,
    Text,
};

// Decodes base64 text, whose padding is optional on the wire. Text that does
// not decode, is too long, is not UTF-8 or holds a control character is
// invalid, which discards the report.
Decoded decodeText(const std::optional<QString> &encoded, int maxDecodedBytes, std::optional<QString> *text)
{
    if (!encoded.has_value() || encoded->isEmpty()) {
        return Decoded::Absent;
    }
    // Unpadded base64 never leaves one character over; Qt would quietly drop it.
    if (encoded->size() % 4 == 1) {
        return Decoded::Invalid;
    }
    const auto decoded = QByteArray::fromBase64Encoding(encoded->toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > maxDecodedBytes) {
        return Decoded::Invalid;
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString result = utf8.decode(decoded.decoded);
    if (utf8.hasError()) {
        return Decoded::Invalid;
    }
    const bool hasControl = std::any_of(result.begin(), result.end(), [](QChar ch) {
        return ch.unicode() <= 0x1f || (ch.unicode() >= 0x7f && ch.unicode() <= 0x9f);
    });
    if (hasControl) {
        return Decoded::Invalid;
    }
    *text = result;
    return Decoded::Text;
}

std::optional<ProgramStatusReport::State> parseState(const QString &value)
{
    static const QHash<QString, ProgramStatusReport::State> states = {
        {QStringLiteral("idle"), ProgramStatusReport::Idle},
        {QStringLiteral("working"), ProgramStatusReport::Working},
        {QStringLiteral("done"), ProgramStatusReport::Done},
        {QStringLiteral("blocked"), ProgramStatusReport::Blocked},
        {QStringLiteral("error"), ProgramStatusReport::Error},
        {QStringLiteral("clear"), ProgramStatusReport::Clear},
    };
    const auto it = states.constFind(value);
    return it == states.cend() ? std::nullopt : std::optional(*it);
}

std::optional<ProgramStatusReport::Kind> parseKind(const QString &value)
{
    if (value == QLatin1String("permission")) {
        return ProgramStatusReport::Permission;
    }
    if (value == QLatin1String("question")) {
        return ProgramStatusReport::Question;
    }
    if (value == QLatin1String("auth")) {
        return ProgramStatusReport::Auth;
    }
    return std::nullopt;
}
}

bool ProgramStatusReport::isFeatureQuery(QStringView body)
{
    return body.trimmed() == u"?";
}

std::optional<ProgramStatusReport> ProgramStatusReport::parse(QStringView body)
{
    if (body.toUtf8().size() + minimumFramingBytes > maxSequenceBytes) {
        return std::nullopt;
    }

    QHash<QString, QString> pairs;
    for (const QStringView rawPair : body.split(QLatin1Char(':'))) {
        const qsizetype equals = rawPair.indexOf(QLatin1Char('='));
        if (equals < 0) {
            continue; // A malformed pair is skipped.
        }
        const QStringView key = rawPair.left(equals).trimmed();
        const QStringView value = rawPair.mid(equals + 1).trimmed();
        // The key limit is a limit, not a syntax rule: it discards the report.
        if (key.size() > maxKeyBytes) {
            return std::nullopt;
        }
        if (key.isEmpty() || !std::all_of(key.begin(), key.end(), isKeyCharacter) || !std::all_of(value.begin(), value.end(), isValueCharacter)) {
            continue;
        }
        // Every occurrence is checked, so a later pair cannot hide a broken limit.
        if (!withinLimits(key, value)) {
            return std::nullopt;
        }
        pairs.insert(key.toString(), value.toString());
    }

    const auto state = parseState(pairs.value(QStringLiteral("state")));
    if (!state.has_value()) {
        return std::nullopt;
    }

    ProgramStatusReport report;
    report.state = *state;

    const auto id = pairs.constFind(QStringLiteral("id"));
    if (id != pairs.cend()) {
        const QStringList segments = id->split(QLatin1Char('/'));
        // An invalid id must not fall back to the root record.
        if (!std::all_of(segments.begin(), segments.end(), [](const QString &segment) {
                return isIdSegment(segment);
            })) {
            return std::nullopt;
        }
        report.id = segments;
    }

    const auto get = [&pairs](const char *key) -> std::optional<QString> {
        const auto it = pairs.constFind(QLatin1String(key));
        return it == pairs.cend() ? std::nullopt : std::optional(*it);
    };

    // Text is decoded even where it goes unused: undecodable text discards the report.
    if (decodeText(get("title"), maxTitleDecodedBytes, &report.title) == Decoded::Invalid
        || decodeText(get("msg"), maxMsgDecodedBytes, &report.msg) == Decoded::Invalid) {
        return std::nullopt;
    }

    if (report.state == Blocked) {
        report.kind = parseKind(pairs.value(QStringLiteral("kind")));
    }

    const QString progress = pairs.value(QStringLiteral("progress"));
    if ((report.state == Working || report.state == Blocked) && !progress.isEmpty() && progress.size() <= 3
        && std::all_of(progress.begin(), progress.end(), [](QChar ch) {
               return ch.isDigit();
           })) {
        const int percent = progress.toInt();
        if (percent <= 100) {
            report.progress = percent;
        }
    }

    const auto app = get("app");
    if (app.has_value() && isIdSegment(*app)) {
        report.app = app;
    }

    return report;
}

bool ProgramStatusReport::operator==(const ProgramStatusReport &other) const
{
    return state == other.state && id == other.id && kind == other.kind && progress == other.progress && app == other.app && title == other.title
        && msg == other.msg;
}
