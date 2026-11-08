/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/ComputerLog.h"

#include "common/Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace deskflow::gui {

namespace {
constexpr int kMaxAddresses = 8;

QString message(const QString &line)
{
  // "[2026-11-07T18:41:02.123] NOTE: client "X" has connected ..." -> the part after "LEVEL: "
  static const QRegularExpression head(QStringLiteral(R"re(^\[[^\]]*\]\s*\w+:\s*(.*)$)re"));
  const auto m = head.match(line.section(QLatin1Char('\n'), 0, 0));
  return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString stripPort(QString addr)
{
  // "192.168.1.111:51234" -> "192.168.1.111"; leave IPv6 "[::1]:1" -> "::1"
  if (addr.startsWith(QLatin1Char('['))) {
    const int end = addr.indexOf(QLatin1Char(']'));
    return end > 0 ? addr.mid(1, end - 1) : addr;
  }
  const int colon = addr.lastIndexOf(QLatin1Char(':'));
  if (colon > 0 && addr.count(QLatin1Char(':')) == 1)
    addr.truncate(colon);
  return addr;
}
} // namespace

ComputerLog::ComputerLog(QObject *parent) : QObject(parent)
{
  load();
}

QString ComputerLog::path()
{
  return Settings::settingsPath() + QStringLiteral("/computers.json");
}

ComputerLog::Entry &ComputerLog::touch(const QString &name, const QString &role)
{
  auto &e = m_entries[name.toLower()];
  if (e.name.isEmpty()) {
    e.name = name;
    e.firstSeen = QDateTime::currentDateTime();
  }
  if (!role.isEmpty())
    e.role = role;
  return e;
}

void ComputerLog::observe(const QString &logLine)
{
  const auto msg = message(logLine);
  if (msg.isEmpty())
    return;

  // Extra screen linked to us (server side)
  static const QRegularExpression connected(
      QStringLiteral(R"re(^client "([^"]+)" has connected from (\S+), screen (\d+x\d+), protocol (\d+\.\d+))re")
  );
  if (const auto m = connected.match(msg); m.hasMatch()) {
    auto &e = touch(m.captured(1), QStringLiteral("extra"));
    const auto addr = stripPort(m.captured(2));
    if (addr != QLatin1String("unknown address")) {
      e.address = addr;
      e.addresses.removeAll(addr);
      e.addresses.prepend(addr);
      while (e.addresses.size() > kMaxAddresses)
        e.addresses.removeLast();
    }
    e.screen = m.captured(3);
    e.protocol = m.captured(4);
    e.lastSeen = QDateTime::currentDateTime();
    e.timesLinked++;
    e.linkedNow = true;
    save();
    Q_EMIT changed(e.name);
    return;
  }

  static const QRegularExpression disconnected(QStringLiteral(R"re(^client "([^"]+)" has disconnected)re"));
  if (const auto m = disconnected.match(msg); m.hasMatch()) {
    const auto key = m.captured(1).toLower();
    if (!m_entries.contains(key))
      return;
    auto &e = m_entries[key];
    if (e.linkedNow && e.lastSeen.isValid())
      e.totalSeconds += e.lastSeen.secsTo(QDateTime::currentDateTime());
    e.linkedNow = false;
    e.lastLeft = QDateTime::currentDateTime();
    save();
    Q_EMIT changed(e.name);
    return;
  }

  // We linked to a main computer (client side)
  static const QRegularExpression toServer(QStringLiteral(R"re(^connected to server '([^']*)' at (\S+):(\d+))re"));
  if (const auto m = toServer.match(msg); m.hasMatch()) {
    const auto host = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
    auto &e = touch(host, QStringLiteral("main"));
    const auto addr = m.captured(2);
    if (addr != QLatin1String("unknown address")) {
      e.address = addr;
      e.addresses.removeAll(addr);
      e.addresses.prepend(addr);
      while (e.addresses.size() > kMaxAddresses)
        e.addresses.removeLast();
    }
    e.protocol = QStringLiteral("port %1").arg(m.captured(3));
    e.lastSeen = QDateTime::currentDateTime();
    e.timesLinked++;
    e.linkedNow = true;
    save();
    Q_EMIT changed(e.name);
    return;
  }

  static const QRegularExpression fromServer(QStringLiteral(R"re(^(disconnected from server|server is dead))re"));
  if (fromServer.match(msg).hasMatch()) {
    for (auto &e : m_entries) {
      if (e.role == QLatin1String("main") && e.linkedNow) {
        if (e.lastSeen.isValid())
          e.totalSeconds += e.lastSeen.secsTo(QDateTime::currentDateTime());
        e.linkedNow = false;
        e.lastLeft = QDateTime::currentDateTime();
        Q_EMIT changed(e.name);
      }
    }
    save();
  }
}

void ComputerLog::allLeft()
{
  bool any = false;
  for (auto &e : m_entries) {
    if (!e.linkedNow)
      continue;
    if (e.lastSeen.isValid())
      e.totalSeconds += e.lastSeen.secsTo(QDateTime::currentDateTime());
    e.linkedNow = false;
    e.lastLeft = QDateTime::currentDateTime();
    any = true;
    Q_EMIT changed(e.name);
  }
  if (any)
    save();
}

void ComputerLog::setFingerprint(const QString &name, const QString &fingerprint)
{
  auto &e = touch(name, QString());
  if (e.fingerprint == fingerprint)
    return;
  e.fingerprint = fingerprint;
  save();
  Q_EMIT changed(e.name);
}

void ComputerLog::forget(const QString &name)
{
  if (m_entries.remove(name.toLower())) {
    save();
    Q_EMIT changed(name);
  }
}

QList<ComputerLog::Entry> ComputerLog::all() const
{
  auto list = m_entries.values();
  std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) { return a.lastSeen > b.lastSeen; });
  return list;
}

QString ComputerLog::report() const
{
  QStringList out;
  out << QStringLiteral("== Computers seen ==");
  for (const auto &e : all()) {
    out << QStringLiteral("%1 (%2)%3").arg(e.name, e.role, e.linkedNow ? QStringLiteral(" — linked now") : QString());
    if (!e.address.isEmpty())
      out << QStringLiteral("  address: %1%2")
                 .arg(
                     e.address, e.addresses.size() > 1
                                    ? QStringLiteral("  (also %1)").arg(e.addresses.mid(1).join(QStringLiteral(", ")))
                                    : QString()
                 );
    if (!e.screen.isEmpty())
      out << QStringLiteral("  screen: %1").arg(e.screen);
    if (!e.protocol.isEmpty())
      out << QStringLiteral("  protocol: %1").arg(e.protocol);
    if (!e.fingerprint.isEmpty())
      out << QStringLiteral("  security code: %1").arg(e.fingerprint);
    out << QStringLiteral("  first seen: %1").arg(e.firstSeen.toString(Qt::ISODate));
    out << QStringLiteral("  last seen: %1").arg(e.lastSeen.toString(Qt::ISODate));
    if (e.lastLeft.isValid())
      out << QStringLiteral("  last left: %1").arg(e.lastLeft.toString(Qt::ISODate));
    out << QStringLiteral("  linked %1 times, %2 h %3 min in total")
               .arg(e.timesLinked)
               .arg(e.totalSeconds / 3600)
               .arg((e.totalSeconds % 3600) / 60);
  }
  if (m_entries.isEmpty())
    out << QStringLiteral("(none yet)");
  return out.join(QLatin1Char('\n'));
}

void ComputerLog::load()
{
  QFile f(path());
  if (!f.open(QIODevice::ReadOnly))
    return;
  const auto doc = QJsonDocument::fromJson(f.readAll());
  for (const auto &v : doc.array()) {
    const auto o = v.toObject();
    Entry e;
    e.name = o.value(QStringLiteral("name")).toString();
    if (e.name.isEmpty())
      continue;
    e.role = o.value(QStringLiteral("role")).toString();
    e.address = o.value(QStringLiteral("address")).toString();
    for (const auto &a : o.value(QStringLiteral("addresses")).toArray())
      e.addresses << a.toString();
    e.screen = o.value(QStringLiteral("screen")).toString();
    e.protocol = o.value(QStringLiteral("protocol")).toString();
    e.fingerprint = o.value(QStringLiteral("fingerprint")).toString();
    e.firstSeen = QDateTime::fromString(o.value(QStringLiteral("firstSeen")).toString(), Qt::ISODate);
    e.lastSeen = QDateTime::fromString(o.value(QStringLiteral("lastSeen")).toString(), Qt::ISODate);
    e.lastLeft = QDateTime::fromString(o.value(QStringLiteral("lastLeft")).toString(), Qt::ISODate);
    e.timesLinked = o.value(QStringLiteral("timesLinked")).toInt();
    e.totalSeconds = static_cast<qint64>(o.value(QStringLiteral("totalSeconds")).toDouble());
    e.linkedNow = false; // nothing is linked when we start
    m_entries.insert(e.name.toLower(), e);
  }
}

void ComputerLog::save() const
{
  QJsonArray arr;
  for (const auto &e : m_entries) {
    QJsonObject o;
    o[QStringLiteral("name")] = e.name;
    o[QStringLiteral("role")] = e.role;
    o[QStringLiteral("address")] = e.address;
    o[QStringLiteral("addresses")] = QJsonArray::fromStringList(e.addresses);
    o[QStringLiteral("screen")] = e.screen;
    o[QStringLiteral("protocol")] = e.protocol;
    o[QStringLiteral("fingerprint")] = e.fingerprint;
    o[QStringLiteral("firstSeen")] = e.firstSeen.toString(Qt::ISODate);
    o[QStringLiteral("lastSeen")] = e.lastSeen.toString(Qt::ISODate);
    o[QStringLiteral("lastLeft")] = e.lastLeft.toString(Qt::ISODate);
    o[QStringLiteral("timesLinked")] = e.timesLinked;
    o[QStringLiteral("totalSeconds")] = static_cast<double>(e.totalSeconds);
    arr.append(o);
  }
  QDir().mkpath(QFileInfo(path()).absolutePath());
  QSaveFile f(path());
  if (!f.open(QIODevice::WriteOnly))
    return;
  f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
  f.commit();
}

} // namespace deskflow::gui
