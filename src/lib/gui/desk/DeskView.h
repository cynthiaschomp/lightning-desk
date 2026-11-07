/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

#include <QFrame>
#include <QHash>
#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QGridLayout;
class ServerConfig;

namespace deskflow::gui {

/**
 * @brief The main window's "Your desk" screen: the computers on the desk arranged around the
 * main one, a sharing switch, and the three always-on conveniences. Replaces the inherited
 * grid/radio-button main window. Pure presentation: MainWindow owns the core and the config
 * and feeds state in through the setters.
 */
class DeskView : public QWidget
{
  Q_OBJECT

public:
  enum class Side
  {
    Above,
    Left,
    Right,
    Below
  };
  Q_ENUM(Side)

  explicit DeskView(QWidget *parent = nullptr);

  /// Server mode: lay out this computer in the middle and the configured computers around it.
  void setDesk(const QString &mainName, const ServerConfig &config);
  /// Client mode: this computer is an extra screen of @p mainName.
  void setLinkedTo(const QString &thisName, const QString &mainName);
  void setConnectedClients(const QStringList &names);
  void setConnectionState(deskflow::core::ConnectionState state);
  void setSharing(bool on);
  void setTrustedCount(int count);
  void refreshConveniences();

Q_SIGNALS:
  void sharingToggled(bool on);
  void addComputerRequested(Side side);
  void computerClicked(const QString &name);
  void settingsRequested();
  void shortcutsRequested();
  void helpRequested();
  void clipboardToggled(bool on);
  void fileDragToggled(bool on);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  struct Card;
  QWidget *buildRail();
  QWidget *buildHeader();
  QWidget *buildGrid();
  QWidget *buildConveniences();
  Card *makeCard(QWidget *parent);
  QWidget *makeEmptyCell(Side side, QWidget *parent);
  void fillCard(Card *card, const QString &name, const QString &subtitle, bool isMain);
  void relayout();
  QString statusFor(const QString &name) const;

  // state
  bool m_serverMode = true;
  bool m_sharing = false;
  QString m_mainName;
  QString m_thisName;
  QHash<Side, QString> m_bySide;
  QHash<QObject *, QString> m_cardNames;
  QStringList m_connected;
  deskflow::core::ConnectionState m_state = deskflow::core::ConnectionState::Disconnected;
  int m_trusted = 0;

  // widgets
  QLabel *m_railStatusDot = nullptr;
  QLabel *m_railStatusText = nullptr;
  QPushButton *m_sharingPill = nullptr;
  QPushButton *m_addButton = nullptr;
  QLabel *m_subtitle = nullptr;
  QGridLayout *m_grid = nullptr;
  QWidget *m_gridHost = nullptr;
  QLabel *m_encryptionValue = nullptr;
  QPushButton *m_clipboardSwitch = nullptr;
  QPushButton *m_fileSwitch = nullptr;
};

} // namespace deskflow::gui
