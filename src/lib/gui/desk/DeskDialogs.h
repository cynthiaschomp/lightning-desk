/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>
#include <QString>

namespace deskflow::gui {

/**
 * @brief "A new computer wants to join" — the branded replacement for the peer-fingerprint prompt.
 * Shows the other computer's security code (from its TLS fingerprint) and asks to allow or not.
 */
class TrustDialog : public QDialog
{
  Q_OBJECT
public:
  enum class Who
  {
    NewComputer, ///< server side: an extra computer is asking to join this desk
    MainComputer ///< client side: this computer is about to link to the main one
  };
  TrustDialog(QWidget *parent, Who who, const QString &name, const QString &fingerprintHex);
};

/**
 * @brief "Where does <name> sit?" — replaces the new-client message box + server config dialog
 * for the common case. Returns the chosen grid cell via cell(), -1 for "not now".
 */
class PlaceDialog : public QDialog
{
  Q_OBJECT
public:
  PlaceDialog(QWidget *parent, const QString &name, const QString &mainName, const QList<int> &takenCells);
  int cell() const
  {
    return m_cell;
  }

private:
  int m_cell = -1;
};

} // namespace deskflow::gui
