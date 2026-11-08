/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Hotkey.h"

#include <QFrame>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;

namespace deskflow::gui {

/**
 * @brief The "Shortcuts" screen: the desk's hotkeys as plain-language rows with keycaps,
 * plus a "New shortcut" flow. Presentation only; MainWindow owns the ServerConfig.
 */
class ShortcutsView : public QWidget
{
  Q_OBJECT
public:
  explicit ShortcutsView(QWidget *parent = nullptr);

  /// @p computers are the names on the desk (main first) for the "Jump to" picker.
  void setShortcuts(const HotkeyList &hotkeys, const QStringList &computers, const QString &mainName);

  /// Plain-language title/subtitle for a hotkey ("Jump to CYN-02", "Moves the cursor there instantly").
  static QPair<QString, QString> describe(const Hotkey &hotkey, const QString &mainName);

Q_SIGNALS:
  void addRequested(const Hotkey &hotkey);
  void removeRequested(int index);

private:
  void rebuild();
  QWidget *makeRow(int index, const Hotkey &hotkey, bool first, bool last, bool suggestion);
  HotkeyList suggestions() const;
  QLabel *m_yoursLabel = nullptr;
  QLabel *m_suggestedLabel = nullptr;

  HotkeyList m_hotkeys;
  QStringList m_computers;
  QString m_mainName;
  QVBoxLayout *m_list = nullptr;
  QWidget *m_listHost = nullptr;
  QLabel *m_empty = nullptr;
  QWidget *m_panelHost = nullptr;
  QPushButton *m_addButton = nullptr;
};

/// "New shortcut": pick what it does, then press the keys. An inline panel, never a window.
class NewShortcutPanel : public QFrame
{
  Q_OBJECT
public:
  NewShortcutPanel(QWidget *parent, const QStringList &computers, const QString &mainName);

Q_SIGNALS:
  void done(const Hotkey &hotkey);
  void cancelled();
};

} // namespace deskflow::gui
