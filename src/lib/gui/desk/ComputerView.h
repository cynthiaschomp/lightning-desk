/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "desk/ComputerLog.h"
#include "gui/config/Computer.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QGridLayout;
class QHBoxLayout;
class QVBoxLayout;

namespace deskflow::gui {

/**
 * @brief One computer's page (opened from its card on the desk; never a sheet or window):
 * where it sits, the other names it answers to, lock-key sync, corner and X11 fixes, forget.
 */
class ComputerView : public QWidget
{
  Q_OBJECT
public:
  explicit ComputerView(QWidget *parent = nullptr);

  /// Show @p computer. @p spots are the free places it could move to (label, cell) and
  /// @p currentCell where it is now. @p status is the pill text ("Linked", "Waiting"…).
  void showComputer(
      const Computer &computer, const QList<QPair<QString, int>> &spots, int currentCell, const QString &status,
      bool linked
  );
  /// Fill the DETAILS section from the connection log.
  void setDetails(const ComputerLog::Entry &entry);
  QString computerName() const
  {
    return m_computer.name();
  }

Q_SIGNALS:
  void backRequested();
  void moveRequested(const QString &name, int cell);
  void forgetRequested(const QString &name);
  /// The computer's own settings changed (aliases, fixes, corners); the caller writes it back.
  void computerChanged(const Computer &computer);

private:
  void rebuildAliases();
  void rebuildSpots();
  void pushChange();

  Computer m_computer;
  QList<QPair<QString, int>> m_spots;
  int m_currentCell = -1;
  bool m_loading = false;

  QLabel *m_title = nullptr;
  QLabel *m_subtitle = nullptr;
  QLabel *m_pill = nullptr;
  QGridLayout *m_spotRow = nullptr;
  QWidget *m_spotHost = nullptr;
  QHBoxLayout *m_aliasRow = nullptr;
  QWidget *m_aliasHost = nullptr;
  QLineEdit *m_aliasEdit = nullptr;
  QPushButton *m_aliasAdd = nullptr;
  QPushButton *m_lockSync = nullptr;
  QPushButton *m_corners = nullptr;
  QSpinBox *m_cornerSize = nullptr;
  QPushButton *m_xtest = nullptr;
  QWidget *m_details = nullptr;
  QList<QPair<QLabel *, QLabel *>> m_detailRows;
};

} // namespace deskflow::gui
