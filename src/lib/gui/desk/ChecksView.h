/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QShowEvent;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace deskflow::gui {

/**
 * @brief The "Something not working?" page (behind Help, never a window). A short list of
 * plain-language checks with a result each, then recent activity in plain language with the raw
 * log one click away. Replaces the inherited log dock on the Desk screen.
 */
class ChecksView : public QWidget
{
  Q_OBJECT
public:
  enum class Status
  {
    Ok,
    Warn,
    Bad,
    Info
  };

  struct Check
  {
    QString title;
    QString detail;
    Status status = Status::Info;
    QString badge;       ///< small accent pill, e.g. "Fixed for you"; empty for none
    QString actionLabel; ///< button text; empty for no button
    QString actionId;    ///< emitted by actionRequested()
  };

  explicit ChecksView(QWidget *parent = nullptr);

  /// Replace the list of checks.
  void setChecks(const QList<Check> &checks);
  /// Feed a raw core/GUI log line. Shown raw, and in plain language when we recognise it.
  void appendLogLine(const QString &line);
  /// Everything on this page as text, for "Copy report".
  QString report() const;
  /// Plain-language reading of a raw log line, or empty if it isn't worth showing.
  static QString plainFor(const QString &line);

Q_SIGNALS:
  void rerunRequested();
  void copyReportRequested();
  void actionRequested(const QString &id);
  void backRequested();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void setRaw(bool raw);
  void refreshActivity();

  QVBoxLayout *m_checkRows = nullptr;
  QList<Check> m_checks;
  QPlainTextEdit *m_activity = nullptr;
  QPushButton *m_rawToggle = nullptr;
  QLabel *m_activityHint = nullptr;
  QStringList m_raw;
  QStringList m_plain;
  bool m_showRaw = false;
};

} // namespace deskflow::gui
