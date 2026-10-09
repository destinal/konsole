// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "claudesessionswidget.h"
#include "claudehooks.h"
#include "claudesessionsmodel.h"
#include "sessionnavigation.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QApplication>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int padding = 6;
constexpr int dotDiameter = 9;
// Longer details end in an ellipsis; the tooltip has all of it.
constexpr int maxDetailLines = 4;

// How wide the detail can be in a row of the given width: what is left
// right of the dot.
int detailWidth(int rowWidth)
{
    return rowWidth - 3 * padding - dotDiameter;
}

// The detail wrapped at word boundaries into at most maxDetailLines lines,
// the last one elided when the rest does not fit.
QStringList wrappedDetail(const QString &detail, const QFont &font, int width)
{
    QStringList lines;
    if (detail.isEmpty() || width <= 0) {
        return lines;
    }
    QTextLayout layout(detail, font);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(textOption);
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        if (lines.size() == maxDetailLines - 1) {
            lines << QFontMetrics(font).elidedText(detail.mid(line.textStart()), Qt::ElideRight, width);
            break;
        }
        lines << detail.mid(line.textStart(), line.textLength()).trimmed();
    }
    layout.endLayout();
    return lines;
}

QString detailText(const QModelIndex &index)
{
    return index.data(ClaudeSessionsModel::DetailRole).toString().simplified();
}

// A dot, the session's title and its status on the first line; then the
// detail, wrapped, on the lines below.
class SessionDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // The row is as wide as the list, which the option does not always say.
        const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
        const int rowWidth = view != nullptr ? view->viewport()->width() : option.rect.width();
        const QFontMetrics metrics(option.font);
        // Always room for one line of detail, so rows keep their height
        // while a session has none.
        const int detailLines = std::max<qsizetype>(1, wrappedDetail(detailText(index), option.font, detailWidth(rowWidth)).size());
        return QSize(rowWidth, 2 * padding + (1 + detailLines) * metrics.lineSpacing());
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

        // Detail, quieter, on the lines below.
        const QStringList detailLines = wrappedDetail(detailText(index), option.font, detailWidth(option.rect.width()));
        painter->setFont(option.font);
        painter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
        for (int i = 0; i < detailLines.size(); ++i) {
            const QRect lineRect(titleLeft, area.top() + (i + 1) * lineHeight, detailWidth(option.rect.width()), lineHeight);
            painter->drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, detailLines.at(i));
        }
        painter->restore();
    }
};
}

ClaudeSessionsWidget::ClaudeSessionsWidget(ClaudeSessionsModel *model, ClaudeHooks *hooks, QWidget *parent)
    : QWidget(parent)
    , _model(model)
    , _hooks(hooks)
    , _hooksBanner(new KMessageWidget(this))
    , _list(new QListView(this))
    , _stack(new QStackedLayout())
{
    // Offer the hooks when they are missing; they are what fills in details
    // and background work.
    _hooksBanner->setMessageType(KMessageWidget::Information);
    _hooksBanner->setWordWrap(true);
    _hooksBanner->setText(i18n("Install the Claude Code hooks to see what Claude is doing in more detail, including background work."));
    auto *install = new QAction(QIcon::fromTheme(QStringLiteral("run-install")), i18n("Install Hooks"), _hooksBanner);
    connect(install, &QAction::triggered, this, [this] {
        QString output;
        if (!_hooks->run(QStringLiteral("install"), &output)) {
            _hooksBanner->setMessageType(KMessageWidget::Error);
            _hooksBanner->setText(output);
        }
    });
    _hooksBanner->addAction(install);
    connect(_hooks, &ClaudeHooks::installedChanged, this, &ClaudeSessionsWidget::updateHooksBanner);

    auto *delegate = new SessionDelegate(_list);
    _list->setModel(model);
    _list->setItemDelegate(delegate);
    _list->setSelectionMode(QAbstractItemView::NoSelection);
    _list->setMouseTracking(true);
    // Rows grow with their detail, and rewrap when the panel is resized.
    _list->setResizeMode(QListView::Adjust);
    connect(model, &QAbstractItemModel::dataChanged, delegate, [delegate](const QModelIndex &topLeft) {
        Q_EMIT delegate->sizeHintChanged(topLeft);
    });
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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_hooksBanner);
    layout->addLayout(_stack, 1);
    updateHooksBanner();

    connect(model, &QAbstractItemModel::modelReset, this, &ClaudeSessionsWidget::updatePlaceholder);
    connect(model, &QAbstractItemModel::rowsInserted, this, &ClaudeSessionsWidget::updatePlaceholder);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &ClaudeSessionsWidget::updatePlaceholder);
    updatePlaceholder();
}

QSize ClaudeSessionsWidget::sizeHint() const
{
    return QSize(fontMetrics().averageCharWidth() * 36, QWidget::sizeHint().height());
}

void ClaudeSessionsWidget::updateHooksBanner()
{
    _hooksBanner->setVisible(!_hooks->isInstalled());
}

void ClaudeSessionsWidget::updatePlaceholder()
{
    _stack->setCurrentWidget(_model->rowCount() > 0 ? static_cast<QWidget *>(_list) : _stack->widget(0));
}

#include "moc_claudesessionswidget.cpp"
