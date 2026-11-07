/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/DeskDialogs.h"

#include "desk/BrandIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;

namespace deskflow::gui {

namespace {

const auto kDialogStyle = QStringLiteral(
    "QDialog{background:#111112;}"
    "QLabel{color:#FFFFFF;background:transparent;}"
    "QPushButton{border:none;border-radius:12px;padding:10px 18px;font-size:14px;font-weight:600;}"
    "QPushButton#primary{background:%1;color:#000000;}"
    "QPushButton#primary:hover{background:#FFE14D;}"
    "QPushButton#secondary{background:%2;color:#FFFFFF;}"
    "QPushButton#secondary:hover{background:%3;}"
    "QPushButton#seg{background:transparent;color:#FFFFFF;border-radius:9px;padding:9px 0;font-size:13px;}"
    "QPushButton#seg:checked{background:%1;color:#000000;}"
    "QPushButton#seg:disabled{color:%4;}"
);

QLabel *text(const QString &t, const QString &color, int px, int weight = 400, QWidget *parent = nullptr)
{
  auto *l = new QLabel(t, parent);
  l->setWordWrap(true);
  l->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;font-weight:%3;").arg(color).arg(px).arg(weight));
  return l;
}

QPushButton *button(const QString &t, const char *id, QWidget *parent)
{
  auto *b = new QPushButton(t, parent);
  b->setObjectName(QLatin1String(id));
  b->setCursor(Qt::PointingHandCursor);
  return b;
}

QWidget *codeRow(const QString &fingerprintHex, QWidget *parent)
{
  auto *w = new QWidget(parent);
  auto *h = new QHBoxLayout(w);
  h->setContentsMargins(0, 6, 0, 6);
  h->setSpacing(10);
  h->addStretch();
  const auto hex = fingerprintHex.toUpper();
  for (int i = 0; i < 4 && i * 4 + 4 <= hex.size(); ++i) {
    auto *c = new QLabel(hex.mid(i * 4, 4), w);
    c->setStyleSheet(QStringLiteral(
        "font-family:ui-monospace,Menlo,Consolas,monospace;font-size:22px;font-weight:600;"
        "letter-spacing:2px;background:#000000;border-radius:10px;padding:10px 14px;color:#FFFFFF;"
    ));
    h->addWidget(c);
  }
  h->addStretch();
  return w;
}

} // namespace

TrustDialog::TrustDialog(QWidget *parent, Who who, const QString &name, const QString &fingerprintHex) : QDialog(parent)
{
  setModal(true);
  setWindowTitle(QStringLiteral("Lightning Desk"));
  setStyleSheet(kDialogStyle.arg(kAccent, kCard2, kLine, kFaint));
  setFixedWidth(500);

  auto *v = new QVBoxLayout(this);
  v->setContentsMargins(26, 26, 26, 22);
  v->setSpacing(14);

  auto *top = new QHBoxLayout();
  top->setSpacing(14);
  auto *icon = new QLabel(this);
  icon->setPixmap(tile(Icon::Laptop, 48, devicePixelRatioF(), QStringLiteral("#3A2450"), kPurple));
  top->addWidget(icon, 0, Qt::AlignTop);
  auto *titles = new QVBoxLayout();
  titles->setSpacing(2);
  const bool joining = who == Who::NewComputer;
  const auto shown = name.isEmpty() ? tr("this computer") : name;
  titles->addWidget(text(joining ? tr("A new computer wants to join") : tr("Link to %1?").arg(shown), kText, 20, 700));
  titles->addWidget(text(
      joining ? (name.isEmpty() ? tr("Found on your network · its name shows once you allow it")
                                : tr("%1 · found on your network").arg(name))
              : tr("%1 is the main computer for this desk").arg(shown),
      kDim, 13
  ));
  top->addLayout(titles, 1);
  v->addLayout(top);

  v->addWidget(text(
      joining ? tr("If this is one of your computers, allow it and it joins your desk. If you weren't expecting "
                   "anything, say no — nothing connects until you do.")
              : tr("Your mouse and keyboard will reach this computer from %1. If that's not your main computer, "
                   "say no.")
                    .arg(shown),
      kMuted, 14
  ));

  v->addWidget(codeRow(fingerprintHex, this));

  auto *note = new QHBoxLayout();
  note->setSpacing(8);
  auto *lock = new QLabel(this);
  lock->setPixmap(brand::icon(Icon::Lock, 14, devicePixelRatioF(), kDim));
  note->addWidget(lock, 0, Qt::AlignTop);
  note->addWidget(
      text(
          tr("This is %1's security code. It never changes unless %1 makes a new key, so you'll only be asked once.")
              .arg(shown),
          kDim, 12
      ),
      1
  );
  v->addLayout(note);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *no = button(tr("Not now"), "secondary", this);
  auto *yes =
      button(joining ? (name.isEmpty() ? tr("Allow it") : tr("Allow %1").arg(name)) : tr("Yes, link"), "primary", this);
  yes->setDefault(true);
  buttons->addWidget(no);
  buttons->addWidget(yes);
  v->addSpacing(4);
  v->addLayout(buttons);

  connect(no, &QPushButton::clicked, this, &QDialog::reject);
  connect(yes, &QPushButton::clicked, this, &QDialog::accept);
}

PlaceDialog::PlaceDialog(QWidget *parent, const QString &name, const QString &mainName, const QList<int> &takenCells)
    : QDialog(parent)
{
  setModal(true);
  setWindowTitle(QStringLiteral("Lightning Desk"));
  setStyleSheet(kDialogStyle.arg(kAccent, kCard2, kLine, kFaint));
  setFixedWidth(460);

  auto *v = new QVBoxLayout(this);
  v->setContentsMargins(26, 26, 26, 22);
  v->setSpacing(14);

  auto *top = new QHBoxLayout();
  top->setSpacing(14);
  auto *icon = new QLabel(this);
  icon->setPixmap(tile(Icon::Laptop, 48, devicePixelRatioF(), kCard2, kAccent));
  top->addWidget(icon, 0, Qt::AlignTop);
  auto *titles = new QVBoxLayout();
  titles->setSpacing(2);
  titles->addWidget(text(tr("Where does %1 sit?").arg(name), kText, 20, 700));
  titles->addWidget(text(tr("Relative to %1, the main computer").arg(mainName), kDim, 13));
  top->addLayout(titles, 1);
  v->addLayout(top);

  v->addWidget(text(
      tr("Pick the edge of %1's screen that %2 is next to. Move the mouse off that edge and it appears there.")
          .arg(mainName, name),
      kMuted, 14
  ));

  auto *segHost = new QWidget(this);
  segHost->setObjectName(QStringLiteral("segHost"));
  segHost->setAttribute(Qt::WA_StyledBackground, true);
  segHost->setStyleSheet(QStringLiteral("QWidget#segHost{background:%1;border-radius:12px;}").arg(kCard));
  auto *seg = new QHBoxLayout(segHost);
  seg->setContentsMargins(4, 4, 4, 4);
  seg->setSpacing(4);
  struct Opt
  {
    QString label;
    int cell;
  };
  // 5 x 3 grid, main computer at index 7.
  const Opt opts[] = {{tr("Above"), 2}, {tr("Left"), 6}, {tr("Right"), 8}, {tr("Below"), 12}};
  QPushButton *firstFree = nullptr;
  for (const auto &o : opts) {
    auto *b = button(o.label, "seg", segHost);
    b->setCheckable(true);
    b->setAutoExclusive(true);
    b->setEnabled(!takenCells.contains(o.cell));
    if (!firstFree && b->isEnabled())
      firstFree = b;
    connect(b, &QPushButton::toggled, this, [this, cell = o.cell](bool on) {
      if (on)
        m_cell = cell;
    });
    seg->addWidget(b, 1);
  }
  if (firstFree)
    firstFree->setChecked(true);
  v->addWidget(segHost);
  v->addWidget(text(tr("You can move it later by dragging its card on your desk."), kDim, 12));

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *no = button(tr("Not now"), "secondary", this);
  auto *yes = button(tr("Add %1").arg(name), "primary", this);
  yes->setDefault(true);
  yes->setEnabled(firstFree != nullptr);
  buttons->addWidget(no);
  buttons->addWidget(yes);
  v->addSpacing(4);
  v->addLayout(buttons);

  connect(no, &QPushButton::clicked, this, [this] {
    m_cell = -1;
    reject();
  });
  connect(yes, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace deskflow::gui
