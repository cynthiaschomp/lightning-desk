/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDateTime>
#include <QString>

namespace deskflow {

/**
 * @brief The plan a copy of Lightning Desk runs under, read from license.json in the settings folder.
 *
 * Free needs no file: a main computer and one other. A signed license.json raises the number of
 * other computers (Plus = 4) and names the tier. The file is signed with Ed25519 by Schomp
 * Technologies; a bad or missing signature means Free.
 */
class License
{
public:
  static constexpr int kFreeExtraComputers = 1;
  static constexpr int kPlusExtraComputers = 4;

  /// Loads (or reloads) license.json from the settings folder. Always returns a usable object.
  static const License &current(bool reload = false);

  /// Read a license from a JSON document. Public for tests and tools.
  static License fromJson(const QByteArray &json);

  bool isLicensed() const
  {
    return m_valid;
  }
  QString tier() const
  {
    return m_valid ? m_tier : QStringLiteral("free");
  }
  QString email() const
  {
    return m_email;
  }
  QDateTime expires() const
  {
    return m_expires;
  }
  /// How many computers besides the main one may connect at once.
  int maxExtraComputers() const
  {
    return m_valid ? m_maxExtra : kFreeExtraComputers;
  }
  /// Why the file was not accepted (empty when valid or absent).
  QString problem() const
  {
    return m_problem;
  }

  static QString licensePath();

private:
  static bool verify(const QByteArray &payload, const QByteArray &signature);

  bool m_valid = false;
  QString m_tier;
  QString m_email;
  QDateTime m_expires;
  int m_maxExtra = kFreeExtraComputers;
  QString m_problem;
};

} // namespace deskflow
