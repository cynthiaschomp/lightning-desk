/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace deskflow::gui {

/**
 * @brief The About / Help page behind the rail's Help entry (never a window): version, update
 * state, this computer's security code, who made it, credits and license, diagnostics.
 */
class AboutView : public QWidget
{
  Q_OBJECT
public:
  explicit AboutView(QWidget *parent = nullptr);

  void setSecurityCode(const QString &sha256Hex);
  void setChecking();
  void setUpdateState(bool ok, bool upToDate, const QString &newest);
  void setDiagnostics(const QString &text);

Q_SIGNALS:
  void checkForUpdatesRequested();
  void getUpdateRequested();
  void openLogsRequested();
  void copyDiagnosticsRequested();

private:
  QLabel *m_updateLine = nullptr;
  QPushButton *m_updateButton = nullptr;
  QLabel *m_code = nullptr;
  QString m_newest;
};

} // namespace deskflow::gui
