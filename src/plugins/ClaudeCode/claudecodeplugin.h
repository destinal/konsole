// SPDX-FileCopyrightText: 2026 Eric Duprey <509630+destinal@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLAUDECODEPLUGIN_H
#define CLAUDECODEPLUGIN_H

#include <pluginsystem/IKonsolePlugin.h>

#include <memory>

namespace Konsole
{
class SessionController;
class MainWindow;
}

class ClaudeCodePlugin : public Konsole::IKonsolePlugin
{
    Q_OBJECT

public:
    ClaudeCodePlugin(QObject *object, const QVariantList &args);
    ~ClaudeCodePlugin() override;

    void createWidgetsForMainWindow(Konsole::MainWindow *mainWindow) override;
    void activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow) override;
    QList<QAction *> menuBarActions(Konsole::MainWindow *mainWindow) const override;

private:
    struct Private;
    std::unique_ptr<Private> priv;
};

#endif // CLAUDECODEPLUGIN_H
