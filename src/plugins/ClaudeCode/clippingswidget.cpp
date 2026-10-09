// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clippingswidget.h"
#include "clippingstore.h"
#include "session/Session.h"
#include "sessionnavigation.h"

#include <KLocalizedString>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace
{
constexpr int padding = 6;
constexpr int detailLines = 3;
constexpr int detailRole = Qt::UserRole + 1;

// A finding: its title in bold, then the start of its detail.
class ClippingDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index)
        return QSize(option.rect.width(), 2 * padding + (1 + detailLines) * QFontMetrics(option.font).lineSpacing());
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        QStyle *style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        painter->save();
        const QRect area = option.rect.adjusted(padding, padding, -padding, -padding);
        const QFontMetrics metrics(option.font);
        const int lineHeight = metrics.lineSpacing();
        const bool selected = option.state & QStyle::State_Selected;

        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(QRect(area.left(), area.top(), area.width(), lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(index.data().toString(), Qt::ElideRight, area.width()));

        painter->setFont(option.font);
        painter->setPen(option.palette.color(selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
        // Wrap the detail into a few lines and elide the last one.
        const QString detail = index.data(detailRole).toString().simplified();
        QString remaining = detail;
        for (int line = 0; line < detailLines && !remaining.isEmpty(); line++) {
            const QRect lineRect(area.left(), area.top() + (line + 1) * lineHeight, area.width(), lineHeight);
            QString text = remaining;
            if (line == detailLines - 1 || metrics.horizontalAdvance(remaining) <= area.width()) {
                text = metrics.elidedText(remaining, Qt::ElideRight, area.width());
                remaining.clear();
            } else {
                // The longest run of whole words that fits.
                int cut = remaining.size();
                while (cut > 0 && metrics.horizontalAdvance(remaining.left(cut)) > area.width()) {
                    const int space = remaining.lastIndexOf(QLatin1Char(' '), cut - 1);
                    cut = space > 0 ? space : cut - 1;
                }
                text = remaining.left(cut);
                remaining = remaining.mid(cut).trimmed();
            }
            painter->drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        painter->restore();
    }
};
}

ClippingsWidget::ClippingsWidget(ClippingStore *store, QWidget *parent)
    : QWidget(parent)
    , _store(store)
    , _heading(new QLabel(this))
    , _list(new QListWidget(this))
    , _send(new QPushButton(QIcon::fromTheme(QStringLiteral("document-send")), i18n("Send to Chat"), this))
    , _archive(new QPushButton(QIcon::fromTheme(QStringLiteral("archive-insert")), i18n("Archive"), this))
    , _stack(new QStackedLayout())
{
    _heading->setWordWrap(true);
    _list->setItemDelegate(new ClippingDelegate(_list));
    _list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _list->setFrameShape(QFrame::NoFrame);
    _send->setToolTip(i18n("Paste the selected findings, or all of them, into the Chat session, and archive them"));
    _archive->setToolTip(i18n("Archive the selected findings, or all of them"));

    auto *placeholder = new QLabel(i18n("No findings. A code review in a Claude workgroup files them here."), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    placeholder->setEnabled(false);
    _stack->addWidget(placeholder);
    _stack->addWidget(_list);

    auto *buttons = new QHBoxLayout();
    buttons->addWidget(_send);
    buttons->addWidget(_archive);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(_heading);
    layout->addLayout(_stack, 1);
    layout->addLayout(buttons);

    connect(_store, &ClippingStore::changed, this, [this](Konsole::Session *session) {
        if (session == _owner) {
            refresh();
        }
    });
    connect(_send, &QPushButton::clicked, this, [this] {
        sendToChat(chosenRows());
    });
    connect(_archive, &QPushButton::clicked, this, [this] {
        if (_owner != nullptr) {
            _store->archive(_owner, chosenRows());
        }
    });
    connect(_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        sendToChat({_list->row(item)});
    });
    refresh();
}

void ClippingsWidget::setOwner(Konsole::Session *owner)
{
    if (_owner == owner) {
        return;
    }
    _owner = owner;
    refresh();
}

Konsole::Session *ClippingsWidget::owner() const
{
    return _owner;
}

QSize ClippingsWidget::sizeHint() const
{
    return QSize(fontMetrics().averageCharWidth() * 36, QWidget::sizeHint().height());
}

QList<int> ClippingsWidget::chosenRows() const
{
    QList<int> rows;
    const auto selected = _list->selectedItems();
    for (QListWidgetItem *item : selected) {
        rows.append(_list->row(item));
    }
    if (rows.isEmpty()) {
        for (int row = 0; row < _list->count(); row++) {
            rows.append(row);
        }
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void ClippingsWidget::sendToChat(const QList<int> &rows)
{
    if (_owner == nullptr || rows.isEmpty()) {
        return;
    }
    const QList<Clipping> live = _store->live(_owner);
    QList<Clipping> chosen;
    for (int row : rows) {
        if (row >= 0 && row < live.size()) {
            chosen.append(live.at(row));
        }
    }
    // Pasted, not submitted: the user reads it over and presses Enter.
    ClaudeCode::pasteIntoSession(_owner, ClippingStore::formatted(chosen), false);
    _store->archive(_owner, rows);
    Q_EMIT sentToChat(_owner);
}

void ClippingsWidget::refresh()
{
    _list->clear();
    const QList<Clipping> live = _owner != nullptr ? _store->live(_owner) : QList<Clipping>();
    for (const Clipping &clipping : live) {
        auto *item = new QListWidgetItem(clipping.title.isEmpty() ? clipping.type : clipping.title, _list);
        item->setData(detailRole, clipping.detail);
        item->setToolTip(clipping.detail);
    }
    _heading->setText(_owner != nullptr ? i18np("1 finding for %2", "%1 findings for %2", live.size(), ClaudeCode::sessionTitle(_owner)) : QString());
    _heading->setVisible(_owner != nullptr && !live.isEmpty());
    _stack->setCurrentIndex(live.isEmpty() ? 0 : 1);
    _send->setEnabled(!live.isEmpty());
    _archive->setEnabled(!live.isEmpty());
}

#include "moc_clippingswidget.cpp"
