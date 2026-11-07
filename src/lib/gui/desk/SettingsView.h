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
class QComboBox;
class QSpinBox;
class QVBoxLayout;

namespace deskflow::gui {

/**
 * @brief The Settings page behind the rail (never a window). Plain-language rows with switches,
 * writing straight to Settings; MainWindow restarts/reloads the core where a change needs it.
 */
class SettingsView : public QWidget
{
  Q_OBJECT
public:
  explicit SettingsView(QWidget *parent = nullptr);

  /// Re-read every control from Settings (after another screen changed something).
  void reload();
  void setPlan(const QString &title, const QString &detail, const QString &pillText);
  void setTrusted(int count, const QStringList &names);

Q_SIGNALS:
  void coreSettingChanged();   ///< something the running core must pick up (restart)
  void serverSettingChanged(); ///< something the server config carries (live reload)
  void planClicked();
  void newKeyRequested();
  void resetRequested();
  void openLogsRequested();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QWidget *section(const QString &title, const QList<QWidget *> &rows);
  QWidget *row(const QString &title, const QString &sub, QWidget *control, bool last);
  QPushButton *makeSwitch(const QString &key, bool invert, bool server);

  bool m_loading = false;
  QList<std::pair<QPushButton *, QString>> m_switches;
  QList<bool> m_switchInverted;
  QLabel *m_planTitle = nullptr;
  QLabel *m_planDetail = nullptr;
  QLabel *m_planPill = nullptr;
  QLabel *m_trustedValue = nullptr;
  QLabel *m_trustedNames = nullptr;
  QComboBox *m_delay = nullptr;
  QComboBox *m_language = nullptr;
  QSpinBox *m_port = nullptr;
  QComboBox *m_interface = nullptr;
  QWidget *m_advanced = nullptr;
  QPushButton *m_advancedToggle = nullptr;
  QWidget *m_resetConfirm = nullptr;
  QWidget *m_planRow = nullptr;
};

} // namespace deskflow::gui
