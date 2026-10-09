// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudesessionswidget.h"
#include "claudesessionsmodel.h"
#include "sessionnavigation.h"

#include <KLocalizedString>

#include <QApplication>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QStackedLayout>
#include <QStyledItemDelegate>

namespace
{
constexpr int padding = 6;
constexpr int dotDiameter = 9;

// Two lines per session: a dot, its title and its status; then the detail.
class SessionDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index)
        const QFontMetrics metrics(option.font);
        return QSize(option.rect.width(), 2 * padding + 2 * metrics.lineSpacing());
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // The panel's background, selection and hover, without the default text.
        opt.text.clear();
        QStyle *style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect area = option.rect.adjusted(padding, padding, -padding, -padding);
        const QFontMetrics metrics(option.font);
        const int lineHeight = metrics.lineSpacing();
        const bool selected = option.state & QStyle::State_Selected;
        const QPalette::ColorGroup group = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        const QColor text = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);

        // Dot.
        const QColor indicator = index.data(ClaudeSessionsModel::IndicatorRole).value<QColor>();
        if (indicator.isValid()) {
            painter->setPen(QPen(indicator.darker(140), 1));
            painter->setBrush(indicator);
            painter->drawEllipse(QRectF(area.left(), area.top() + (lineHeight - dotDiameter) / 2.0, dotDiameter, dotDiameter));
        }

        // Status, right-aligned in its own color.
        const QString status = index.data(ClaudeSessionsModel::StatusRole).toString();
        QColor statusColor = index.data(ClaudeSessionsModel::StatusColorRole).value<QColor>();
        if (!statusColor.isValid() || selected) {
            statusColor = text;
        }
        const int statusWidth = metrics.horizontalAdvance(status);
        painter->setPen(statusColor);
        painter->drawText(QRect(area.right() - statusWidth, area.top(), statusWidth, lineHeight), Qt::AlignRight | Qt::AlignVCenter, status);

        // Title, bold, in what is left of the first line.
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(text);
        const int titleLeft = area.left() + dotDiameter + padding;
        const QRect titleRect(titleLeft, area.top(), area.right() - statusWidth - padding - titleLeft, lineHeight);
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(index.data().toString(), Qt::ElideRight, titleRect.width()));

        // Detail, quieter, on the second line.
        const QString detail = index.data(ClaudeSessionsModel::DetailRole).toString().simplified();
        if (!detail.isEmpty()) {
            painter->setFont(option.font);
            painter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
            const QRect detailRect(titleLeft, area.top() + lineHeight, area.right() - titleLeft, lineHeight);
            painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(detail, Qt::ElideRight, detailRect.width()));
        }
        painter->restore();
    }
};
}

ClaudeSessionsWidget::ClaudeSessionsWidget(ClaudeSessionsModel *model, QWidget *parent)
    : QWidget(parent)
    , _model(model)
    , _list(new QListView(this))
    , _stack(new QStackedLayout(this))
{
    _list->setModel(model);
    _list->setItemDelegate(new SessionDelegate(_list));
    _list->setSelectionMode(QAbstractItemView::NoSelection);
    _list->setMouseTracking(true);
    _list->setUniformItemSizes(true);
    _list->setFrameShape(QFrame::NoFrame);
    connect(_list, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (Konsole::Session *session = _model->session(index)) {
            ClaudeCode::activateSession(session);
        }
    });

    auto *placeholder = new QLabel(i18n("No Claude Code sessions"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    placeholder->setEnabled(false);

    _stack->addWidget(placeholder);
    _stack->addWidget(_list);

    connect(model, &QAbstractItemModel::modelReset, this, &ClaudeSessionsWidget::updatePlaceholder);
    connect(model, &QAbstractItemModel::rowsInserted, this, &ClaudeSessionsWidget::updatePlaceholder);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &ClaudeSessionsWidget::updatePlaceholder);
    updatePlaceholder();
}

QSize ClaudeSessionsWidget::sizeHint() const
{
    return QSize(fontMetrics().averageCharWidth() * 36, QWidget::sizeHint().height());
}

void ClaudeSessionsWidget::updatePlaceholder()
{
    _stack->setCurrentWidget(_model->rowCount() > 0 ? static_cast<QWidget *>(_list) : _stack->widget(0));
}

#include "moc_claudesessionswidget.cpp"
