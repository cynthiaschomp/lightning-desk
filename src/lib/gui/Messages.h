/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QMessageLogContext>
#include <QString>

#include <common/Enums.h>

class QWidget;

#include <functional>

namespace deskflow::gui::messages {

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

void raiseCriticalDialog();

/// Where non-fatal critical errors go instead of a message box (an inline notice on the Desk
/// screen). While a sink is set, no window pops up for them. Pass nullptr to restore the box.
using CriticalSink = std::function<void(const QString &message, const QString &fileLine)>;
void setCriticalSink(CriticalSink sink);

void showFirstServerStartMessage(QWidget *parent);

void showFirstConnectedMessage(QWidget *parent);

void showCloseReminder(QWidget *parent);

bool showNewClientPrompt(QWidget *parent, const QString &clientName);

bool showUpdateCheckOption(QWidget *parent);

bool showDaemonOffline(QWidget *parent);

} // namespace deskflow::gui::messages
