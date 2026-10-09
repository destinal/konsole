// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLIPPINGSWIDGET_H
#define CLIPPINGSWIDGET_H

#include <QPointer>
#include <QWidget>

namespace Konsole
{
class Session;
}
class ClippingStore;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedLayout;

/**
 * The Claude Clippings panel: the findings a code review filed for the
 * workgroup the window is showing. Send to Chat pastes them into the Chat
 * session, without submitting, and archives them; Archive just archives.
 * With nothing selected, either acts on every finding.
 */
class ClippingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClippingsWidget(ClippingStore *store, QWidget *parent = nullptr);

    /**
     * Shows the clippings filed with @p owner, the session that keeps them
     * (a workgroup's Chat session), which is also where they are sent.
     */
    void setOwner(Konsole::Session *owner);
    Konsole::Session *owner() const;

    QSize sizeHint() const override;

Q_SIGNALS:
    /** Findings were pasted into @p owner, which should now be brought forward. */
    void sentToChat(Konsole::Session *owner);

private:
    void refresh();
    QList<int> chosenRows() const;
    void sendToChat(const QList<int> &rows);

    ClippingStore *_store;
    QPointer<Konsole::Session> _owner;
    QLabel *_heading;
    QListWidget *_list;
    QPushButton *_send;
    QPushButton *_archive;
    QStackedLayout *_stack;
};

#endif // CLIPPINGSWIDGET_H
