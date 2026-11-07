/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/BrandIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

namespace deskflow::gui::brand {

namespace {

void drawIcon(QPainter &p, Icon which, const QString &color)
{
  QPen pen(QColor(color), 2.0);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  switch (which) {
  case Icon::Bolt: {
    const QPolygonF bolt{{13, 2}, {4, 14}, {11, 14}, {10, 22}, {19, 10}, {12, 10}, {13, 2}};
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(color));
    p.drawPolygon(bolt);
    break;
  }
  case Icon::Monitor:
    p.drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
    p.drawLine(QPointF(8, 21), QPointF(16, 21));
    p.drawLine(QPointF(12, 17), QPointF(12, 21));
    break;
  case Icon::Laptop:
    p.drawRoundedRect(QRectF(3, 4, 18, 12), 2, 2);
    p.drawLine(QPointF(2, 20), QPointF(22, 20));
    break;
  case Icon::Keyboard:
    p.drawRoundedRect(QRectF(2, 6, 20, 12), 2, 2);
    for (int i = 0; i < 4; ++i)
      p.drawPoint(QPointF(6 + i * 4, 10));
    p.drawLine(QPointF(7, 14), QPointF(17, 14));
    break;
  case Icon::Gear: {
    p.drawEllipse(QPointF(12, 12), 3, 3);
    for (int i = 0; i < 8; ++i) {
      p.save();
      p.translate(12, 12);
      p.rotate(i * 45.0);
      p.drawLine(QPointF(0, -7), QPointF(0, -9.5));
      p.restore();
    }
    p.drawEllipse(QPointF(12, 12), 7, 7);
    break;
  }
  case Icon::Help:
    p.drawEllipse(QPointF(12, 12), 10, 10);
    p.drawPoint(QPointF(12, 17));
    {
      QPainterPath q;
      q.moveTo(9.5, 9.5);
      q.cubicTo(9.5, 7.5, 14.5, 7.5, 14.5, 9.5);
      q.cubicTo(14.5, 11.5, 12, 11, 12, 13.5);
      p.drawPath(q);
    }
    break;
  case Icon::Plus:
    p.drawLine(QPointF(12, 5), QPointF(12, 19));
    p.drawLine(QPointF(5, 12), QPointF(19, 12));
    break;
  case Icon::Clipboard:
    p.drawRoundedRect(QRectF(9, 9, 13, 13), 2, 2);
    {
      QPainterPath c;
      c.moveTo(5, 15);
      c.lineTo(4, 15);
      c.cubicTo(2.9, 15, 2, 14.1, 2, 13);
      c.lineTo(2, 4);
      c.cubicTo(2, 2.9, 2.9, 2, 4, 2);
      c.lineTo(13, 2);
      c.cubicTo(14.1, 2, 15, 2.9, 15, 4);
      c.lineTo(15, 5);
      p.drawPath(c);
    }
    break;
  case Icon::File: {
    QPainterPath f;
    f.moveTo(14, 2);
    f.lineTo(6, 2);
    f.cubicTo(4.9, 2, 4, 2.9, 4, 4);
    f.lineTo(4, 20);
    f.cubicTo(4, 21.1, 4.9, 22, 6, 22);
    f.lineTo(18, 22);
    f.cubicTo(19.1, 22, 20, 21.1, 20, 20);
    f.lineTo(20, 8);
    f.closeSubpath();
    p.drawPath(f);
    p.drawLine(QPointF(14, 2), QPointF(14, 8));
    p.drawLine(QPointF(14, 8), QPointF(20, 8));
    break;
  }
  case Icon::Lock:
    p.drawRoundedRect(QRectF(3, 11, 18, 11), 2, 2);
    {
      QPainterPath a;
      a.moveTo(7, 11);
      a.lineTo(7, 7);
      a.arcTo(QRectF(7, 2, 10, 10), 180, -180);
      a.lineTo(17, 11);
      p.drawPath(a);
    }
    break;
  case Icon::ArrowUp:
    p.drawLine(QPointF(12, 19), QPointF(12, 5));
    p.drawLine(QPointF(6, 11), QPointF(12, 5));
    p.drawLine(QPointF(18, 11), QPointF(12, 5));
    break;
  case Icon::ArrowDown:
    p.drawLine(QPointF(12, 5), QPointF(12, 19));
    p.drawLine(QPointF(6, 13), QPointF(12, 19));
    p.drawLine(QPointF(18, 13), QPointF(12, 19));
    break;
  case Icon::ArrowLeft:
    p.drawLine(QPointF(19, 12), QPointF(5, 12));
    p.drawLine(QPointF(11, 6), QPointF(5, 12));
    p.drawLine(QPointF(11, 18), QPointF(5, 12));
    break;
  case Icon::ArrowRight:
    p.drawLine(QPointF(5, 12), QPointF(19, 12));
    p.drawLine(QPointF(13, 6), QPointF(19, 12));
    p.drawLine(QPointF(13, 18), QPointF(19, 12));
    break;
  }
}

} // namespace

QPixmap icon(Icon which, int size, qreal dpr, const QString &color)
{
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.scale(size / 24.0, size / 24.0);
  drawIcon(p, which, color);
  return pm;
}

QPixmap tile(Icon which, int size, qreal dpr, const QString &bg, const QString &fg)
{
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(bg));
  p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);
  const qreal inner = size * 0.55;
  p.translate((size - inner) / 2, (size - inner) / 2);
  p.scale(inner / 24.0, inner / 24.0);
  drawIcon(p, which, fg);
  return pm;
}

} // namespace deskflow::gui::brand
