/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QPixmap>
#include <QString>

/// Brand palette and the small set of line icons the branded screens draw with QPainter,
/// so no SVG module or icon theme is needed on any platform.
namespace deskflow::gui::brand {

inline const auto kBg = QStringLiteral("#000000");
inline const auto kCard = QStringLiteral("#1C1C1E");
inline const auto kCard2 = QStringLiteral("#2C2C2E");
inline const auto kLine = QStringLiteral("#3A3A3C");
inline const auto kText = QStringLiteral("#FFFFFF");
inline const auto kMuted = QStringLiteral("#AEAEB2");
inline const auto kDim = QStringLiteral("#A1A1A6");
inline const auto kFaint = QStringLiteral("#636366");
inline const auto kAccent = QStringLiteral("#FFD60A");
inline const auto kGreen = QStringLiteral("#34C759");
inline const auto kRed = QStringLiteral("#FF453A");
inline const auto kPurple = QStringLiteral("#BF5AF2");

enum class Icon
{
  Bolt,
  Monitor,
  Laptop,
  Keyboard,
  Gear,
  Help,
  Plus,
  Clipboard,
  File,
  Lock,
  ArrowUp,
  ArrowDown,
  ArrowLeft,
  ArrowRight
};

/// 24-unit line icon rendered at @p size device-independent pixels in @p color.
QPixmap icon(Icon which, int size, qreal dpr, const QString &color = kText);

/// Rounded square tile with an icon centred in it (the 36/40px tiles on cards).
QPixmap tile(Icon which, int size, qreal dpr, const QString &bg = kCard2, const QString &fg = kText);

} // namespace deskflow::gui::brand
