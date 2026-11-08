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
class QPlainTextEdit;
class QFrame;

namespace deskflow::gui {

/**
 * @brief The Plan page (canvas App-Plan + App-Activate), reached from Settings > Your plan.
 * Shows the current plan, Free vs Plus side by side, and an inline "I have a license" panel
 * that pastes/validates a signed license and saves it. No windows, no dialogs.
 */
class PlanView : public QWidget
{
  Q_OBJECT
public:
  explicit PlanView(QWidget *parent = nullptr);

  /// Re-read license.json and refresh every card.
  void reload(int computersOnDesk);

Q_SIGNALS:
  void getPlusRequested();
  void licenseSaved();
  void backRequested();

private:
  QLabel *m_currentTitle = nullptr;
  QLabel *m_currentDetail = nullptr;
  QLabel *m_currentPill = nullptr;
  QFrame *m_freeCard = nullptr;
  QFrame *m_plusCard = nullptr;
  QPushButton *m_plusButton = nullptr;
  QWidget *m_activatePanel = nullptr;
  QPlainTextEdit *m_licenseText = nullptr;
  QLabel *m_activateStatus = nullptr;
  QPushButton *m_activateButton = nullptr;
};

} // namespace deskflow::gui
