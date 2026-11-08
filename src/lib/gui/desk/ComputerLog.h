/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace deskflow::gui {

/**
 * @brief What we know about every computer that has ever linked to this one (or that this one has
 * linked to). Filled from the core's log lines, kept in computers.json next to the settings, shown
 * on the Computer page and in the Checks report.
 */
class ComputerLog : public QObject
{
  Q_OBJECT
public:
  struct Entry
  {
    QString name;
    QString role;          ///< "extra" (it linked to us) or "main" (we linked to it)
    QString address;       ///< last address seen
    QStringList addresses; ///< every distinct address, newest first (max 8)
    QString screen;        ///< "1920x1080"
    QString protocol;      ///< "1.8"
    QString fingerprint;   ///< security code, when the trust flow told us
    QDateTime firstSeen;
    QDateTime lastSeen;
    QDateTime lastLeft;
    int timesLinked = 0;
    qint64 totalSeconds = 0; ///< time linked over all sessions
    bool linkedNow = false;
  };

  explicit ComputerLog(QObject *parent = nullptr);

  /// Feed every core/GUI log line; the ones we understand update the log.
  void observe(const QString &logLine);
  /// Mark every computer as not linked (core stopped).
  void allLeft();
  /// Remember a security code for @p name (from the trust notice).
  void setFingerprint(const QString &name, const QString &fingerprint);
  /// Drop a computer (it was forgotten from the desk).
  void forget(const QString &name);

  bool has(const QString &name) const
  {
    return m_entries.contains(name.toLower());
  }
  Entry entry(const QString &name) const
  {
    return m_entries.value(name.toLower());
  }
  QList<Entry> all() const;
  /// Plain-text table for the Checks report.
  QString report() const;
  static QString path();

Q_SIGNALS:
  void changed(const QString &name);

private:
  void load();
  void save() const;
  Entry &touch(const QString &name, const QString &role);

  QHash<QString, Entry> m_entries; // key: lower-case name
};

} // namespace deskflow::gui
