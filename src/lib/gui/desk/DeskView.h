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
class QVBoxLayout;
class QStackedWidget;
class ServerConfig;

namespace deskflow::gui {

class ShortcutsView;
class SettingsView;
class AboutView;
class NoticeCard;

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
  /// The Shortcuts page (hosted behind the rail's Shortcuts entry).
  ShortcutsView *shortcuts() const
  {
    return m_shortcuts;
  }
  SettingsView *settings() const
  {
    return m_settings;
  }
  AboutView *about() const
  {
    return m_about;
  }
  void showSettingsPage();
  void showAboutPage();
  void showDeskPage();
  /// Show an inline notice at the top of the Desk page (replaces every dialog). The card is
  /// deleted when answered or when dismissNotice() is called; only one is shown at a time.
  NoticeCard *notice();
  bool hasNotice() const;
  void dismissNotice();

Q_SIGNALS:
  void sharingToggled(bool on);
  /// Add a computer into grid cell @p cell (the setup wizard places it there once linked).
  void addComputerRequested(int cell);
  void computerClicked(const QString &name);
  void moveRequested(const QString &name, int cell);
  void forgetRequested(const QString &name);
  void settingsRequested();
  void shortcutsRequested();
  void helpRequested();
  void clipboardToggled(bool on);
  void fileDragToggled(bool on);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Card;
  QWidget *buildRail();
  QWidget *buildHeader();
  QWidget *buildGrid();
  QWidget *buildConveniences();
  Card *makeCard(QWidget *parent);
  QWidget *makeEmptyCell(int cell, QWidget *parent);
  void fillCard(Card *card, const QString &name, const QString &subtitle, bool isMain);
  void relayout();
  QString statusFor(const QString &name) const;
  QString placeLabel(int cell) const;
  bool occupied(int cell) const;
  bool nextToOccupied(int cell) const;
  int firstFreeNearMain() const;
  void showCardMenu(const QString &name, const QPoint &globalPos);

  // state
  bool m_serverMode = true;
  bool m_sharing = false;
  QString m_mainName;
  QString m_thisName;
  int m_columns = 5;
  int m_rows = 3;
  int m_mainCell = 7;
  QHash<int, QString> m_cells; // grid index -> computer name (main included)
  QHash<QObject *, QString> m_cardNames;
  QStringList m_connected;
  deskflow::core::ConnectionState m_state = deskflow::core::ConnectionState::Disconnected;
  int m_trusted = 0;
  int m_cardW = 160;
  int m_shownColumns = 0;

  // widgets
  QLabel *m_railStatusDot = nullptr;
  QLabel *m_railStatusText = nullptr;
  QPushButton *m_sharingPill = nullptr;
  QPushButton *m_addButton = nullptr;
  QLabel *m_subtitle = nullptr;
  QGridLayout *m_grid = nullptr;
  QWidget *m_gridHost = nullptr;
  QStackedWidget *m_pages = nullptr;
  QVBoxLayout *m_noticeHost = nullptr;
  NoticeCard *m_notice = nullptr;
  QWidget *m_deskPage = nullptr;
  ShortcutsView *m_shortcuts = nullptr;
  SettingsView *m_settings = nullptr;
  AboutView *m_about = nullptr;
  QPushButton *m_navHelp = nullptr;
  QPushButton *m_navSettings = nullptr;
  QPushButton *m_navDesk = nullptr;
  QPushButton *m_navShortcuts = nullptr;
  QLabel *m_encryptionValue = nullptr;
  QPushButton *m_clipboardSwitch = nullptr;
  QPushButton *m_fileSwitch = nullptr;
};

} // namespace deskflow::gui
