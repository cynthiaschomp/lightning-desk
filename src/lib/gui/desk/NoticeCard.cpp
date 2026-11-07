/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/NoticeCard.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;

namespace deskflow::gui {

namespace {

QLabel *label(const QString &t, const QString &color, int px, int weight, QWidget *parent)
{
  auto *l = new QLabel(t, parent);
  l->setWordWrap(true);
  l->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;font-weight:%3;background:transparent;border:none;")
                       .arg(color)
                       .arg(px)
                       .arg(weight));
  return l;
}

void clearLayout(QLayout *layout)
{
  while (auto *item = layout->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->hide();
      w->deleteLater();
    }
    delete item;
  }
}

} // namespace

NoticeCard::NoticeCard(QWidget *parent) : QFrame(parent)
{
  setObjectName(QStringLiteral("notice"));
  setStyleSheet(
      QStringLiteral(
          "QFrame#notice{background:#111112;border:1px solid rgba(255,214,10,0.45);border-radius:14px;}"
          "QLabel{border:none;}"
          "QPushButton{border:none;border-radius:11px;padding:8px 16px;font-size:13px;font-weight:600;}"
          "QPushButton#primary{background:%1;color:#000000;}QPushButton#primary:hover{background:#FFE14D;}"
          "QPushButton#danger{background:%2;color:#FFFFFF;}QPushButton#danger:hover{background:#FF6B61;}"
          "QPushButton#secondary{background:%3;color:#FFFFFF;}QPushButton#secondary:hover{background:%4;}"
          "QPushButton#seg{background:transparent;color:#FFFFFF;border-radius:8px;padding:7px 0;font-size:12px;}"
          "QPushButton#seg:checked{background:%1;color:#000000;}QPushButton#seg:disabled{color:%5;}"
      )
          .arg(kAccent, kRed, kCard2, kLine, kFaint)
  );

  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(18, 16, 18, 14);
  outer->setSpacing(14);

  m_iconHost = new QWidget(this);
  m_iconHost->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *ih = new QVBoxLayout(m_iconHost);
  ih->setContentsMargins(0, 2, 0, 0);
  m_icon = new QLabel(m_iconHost);
  m_icon->setStyleSheet(QStringLiteral("background:transparent;"));
  ih->addWidget(m_icon, 0, Qt::AlignTop);
  ih->addStretch();
  outer->addWidget(m_iconHost, 0, Qt::AlignTop);

  m_body = new QVBoxLayout();
  m_body->setSpacing(8);
  m_title = label(QString(), kText, 16, 700, this);
  m_text = label(QString(), kMuted, 13, 400, this);
  m_body->addWidget(m_title);
  m_body->addWidget(m_text);

  m_codeRow = new QWidget(this);
  m_codeRow->setStyleSheet(QStringLiteral("background:transparent;"));
  new QHBoxLayout(m_codeRow);
  m_codeRow->hide();
  m_body->addWidget(m_codeRow);

  m_choiceRow = new QWidget(this);
  m_choiceRow->setObjectName(QStringLiteral("segHost"));
  m_choiceRow->setAttribute(Qt::WA_StyledBackground, true);
  m_choiceRow->setStyleSheet(QStringLiteral("QWidget#segHost{background:%1;border-radius:11px;}").arg(kCard));
  new QHBoxLayout(m_choiceRow);
  m_choices = new QButtonGroup(this);
  m_choiceRow->hide();
  m_body->addWidget(m_choiceRow);

  m_noteRow = new QWidget(this);
  m_noteRow->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *nh = new QHBoxLayout(m_noteRow);
  nh->setContentsMargins(0, 0, 0, 0);
  nh->setSpacing(8);
  auto *lock = new QLabel(m_noteRow);
  lock->setPixmap(icon(Icon::Lock, 14, devicePixelRatioF(), kDim));
  lock->setStyleSheet(QStringLiteral("background:transparent;"));
  nh->addWidget(lock, 0, Qt::AlignTop);
  m_note = label(QString(), kDim, 12, 400, m_noteRow);
  nh->addWidget(m_note, 1);
  m_noteRow->hide();
  m_body->addWidget(m_noteRow);

  m_buttonRow = new QWidget(this);
  m_buttonRow->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *bh = new QHBoxLayout(m_buttonRow);
  bh->setContentsMargins(0, 4, 0, 0);
  bh->setSpacing(8);
  bh->addStretch();
  m_buttonRow->hide();
  m_body->addWidget(m_buttonRow);

  outer->addLayout(m_body, 1);
  setIcon(Icon::Monitor);
}

NoticeCard *NoticeCard::setIcon(Icon which, Tone tone)
{
  const QString bg = tone == Tone::Danger   ? QStringLiteral("#3A1F1E")
                     : tone == Tone::Accent ? QStringLiteral("#3A2450")
                                            : kCard2;
  const QString fg = tone == Tone::Danger ? kRed : tone == Tone::Accent ? kPurple : kAccent;
  m_icon->setPixmap(tile(which, 44, devicePixelRatioF(), bg, fg));
  return this;
}

NoticeCard *NoticeCard::setTitle(const QString &title)
{
  m_title->setText(title);
  return this;
}

NoticeCard *NoticeCard::setBody(const QString &body)
{
  m_text->setText(body);
  m_text->setVisible(!body.isEmpty());
  return this;
}

NoticeCard *NoticeCard::setCode(const QString &hex)
{
  auto *h = static_cast<QHBoxLayout *>(m_codeRow->layout());
  clearLayout(h);
  h->setContentsMargins(0, 2, 0, 2);
  h->setSpacing(10);
  const auto up = hex.toUpper();
  for (int i = 0; i < 4 && i * 4 + 4 <= up.size(); ++i) {
    auto *c = new QLabel(up.mid(i * 4, 4), m_codeRow);
    c->setStyleSheet(QStringLiteral(
        "font-family:ui-monospace,Menlo,Consolas,monospace;font-size:20px;font-weight:600;"
        "letter-spacing:2px;background:#000000;border-radius:10px;padding:8px 12px;color:#FFFFFF;"
    ));
    h->addWidget(c);
  }
  h->addStretch();
  m_codeRow->setVisible(!hex.isEmpty());
  return this;
}

NoticeCard *NoticeCard::setChoices(const QList<Choice> &choices, int selectedValue)
{
  auto *h = static_cast<QHBoxLayout *>(m_choiceRow->layout());
  clearLayout(h);
  h->setContentsMargins(4, 4, 4, 4);
  h->setSpacing(4);
  for (auto *b : m_choices->buttons())
    m_choices->removeButton(b);
  QPushButton *firstFree = nullptr;
  for (const auto &c : choices) {
    auto *b = new QPushButton(c.label, m_choiceRow);
    b->setStyleSheet(QStringLiteral(
                         "QPushButton{background:transparent;color:#FFFFFF;border:none;border-radius:8px;"
                         "padding:7px 0;font-size:12px;}QPushButton:checked{background:%1;color:#000000;}"
                         "QPushButton:disabled{color:%2;}"
    )
                         .arg(kAccent, kFaint));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setEnabled(c.enabled);
    m_choices->addButton(b, c.value);
    h->addWidget(b, 1);
    if (c.enabled && (c.value == selectedValue || (!firstFree && selectedValue < 0)))
      firstFree = b;
  }
  if (firstFree)
    firstFree->setChecked(true);
  m_choiceRow->setVisible(!choices.isEmpty());
  return this;
}

NoticeCard *NoticeCard::setFootnote(const QString &note)
{
  m_note->setText(note);
  m_noteRow->setVisible(!note.isEmpty());
  return this;
}

NoticeCard *NoticeCard::setButtons(const QStringList &labels, bool primaryIsDanger)
{
  auto *h = static_cast<QHBoxLayout *>(m_buttonRow->layout());
  clearLayout(h);
  h->addStretch();
  for (int i = 0; i < labels.size(); ++i) {
    auto *b = new QPushButton(labels[i], m_buttonRow);
    const bool primary = i == labels.size() - 1;
    const QString bg = primary ? (primaryIsDanger ? kRed : kAccent) : kCard2;
    const QString fg = primary && !primaryIsDanger ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF");
    const QString hover = primary ? (primaryIsDanger ? QStringLiteral("#FF6B61") : QStringLiteral("#FFE14D")) : kLine;
    b->setStyleSheet(QStringLiteral(
                         "QPushButton{background:%1;color:%2;border:none;border-radius:11px;padding:8px 16px;"
                         "font-size:13px;font-weight:600;}QPushButton:hover{background:%3;}"
    )
                         .arg(bg, fg, hover));
    b->setCursor(Qt::PointingHandCursor);
    connect(b, &QPushButton::clicked, this, [this, i] { Q_EMIT answered(i, chosenValue()); });
    h->addWidget(b);
  }
  m_buttonRow->setVisible(!labels.isEmpty());
  return this;
}

int NoticeCard::chosenValue() const
{
  return m_choices->checkedId();
}

} // namespace deskflow::gui
