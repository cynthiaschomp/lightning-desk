/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/PlanView.h"

#include "desk/BrandIcons.h"
#include "net/License.h"

#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;

namespace deskflow::gui {

namespace {

QLabel *text(const QString &t, const QString &color, int px, int weight = 400, QWidget *parent = nullptr)
{
  auto *l = new QLabel(t, parent);
  l->setWordWrap(true);
  l->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;font-weight:%3;background:transparent;border:none;")
                       .arg(color)
                       .arg(px)
                       .arg(weight));
  return l;
}

QPushButton *button(const QString &t, bool primary, QWidget *parent)
{
  auto *b = new QPushButton(t, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{background:%1;color:%2;border:none;border-radius:11px;padding:9px 18px;"
                       "font-size:13px;font-weight:600;}QPushButton:hover{background:%3;}"
                       "QPushButton:disabled{background:%4;color:%5;}"
  )
                       .arg(
                           primary ? kAccent : kCard2, primary ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF"),
                           primary ? QStringLiteral("#FFE14D") : kLine, kCard2, kFaint
                       ));
  return b;
}

QWidget *bullet(const QString &t, bool included, QWidget *parent)
{
  auto *w = new QWidget(parent);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *h = new QHBoxLayout(w);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(8);
  h->addWidget(text(included ? QStringLiteral("✓") : QStringLiteral("–"), included ? kGreen : kFaint, 13, 700, w));
  h->addWidget(text(t, included ? kText : kDim, 13, 400, w), 1);
  return w;
}

QFrame *planCard(
    const QString &name, const QString &price, const QString &priceSub, const QStringList &perks, int includedCount,
    bool accent, QWidget *parent, QVBoxLayout **footer
)
{
  auto *card = new QFrame(parent);
  card->setObjectName(QStringLiteral("planCard"));
  card->setStyleSheet(
      QStringLiteral("QFrame#planCard{background:%1;border:%2;border-radius:16px;}QLabel{border:none;}")
          .arg(kCard, accent ? QStringLiteral("2px solid %1").arg(kAccent) : QStringLiteral("1px solid %1").arg(kLine))
  );
  auto *v = new QVBoxLayout(card);
  v->setContentsMargins(20, 18, 20, 18);
  v->setSpacing(10);
  auto *top = new QHBoxLayout();
  top->addWidget(text(name, kText, 18, 700, card));
  top->addStretch();
  if (accent) {
    auto *pill = text(QObject::tr("BEST FOR 3+"), kBg, 10, 700, card);
    pill->setStyleSheet(QStringLiteral(
                            "color:%1;background:%2;border-radius:8px;padding:3px 8px;font-size:10px;"
                            "font-weight:700;"
    )
                            .arg(kBg, kAccent));
    top->addWidget(pill);
  }
  v->addLayout(top);
  auto *priceRow = new QHBoxLayout();
  priceRow->setSpacing(6);
  priceRow->addWidget(text(price, kText, 28, 700, card));
  priceRow->addWidget(text(priceSub, kDim, 12, 400, card), 1, Qt::AlignBottom);
  v->addLayout(priceRow);
  v->addSpacing(4);
  for (int i = 0; i < perks.size(); ++i)
    v->addWidget(bullet(perks[i], i < includedCount, card));
  v->addStretch(1);
  *footer = v;
  return card;
}

} // namespace

PlanView::PlanView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("planView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#planView{background:%1;}").arg(kBg));

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);

  auto *hdr = new QHBoxLayout();
  auto *back = button(tr("‹ Settings"), false, content);
  connect(back, &QPushButton::clicked, this, &PlanView::backRequested);
  hdr->addWidget(back);
  hdr->addSpacing(8);
  hdr->addWidget(text(tr("Your plan"), kText, 22, 700, content), 1);
  col->addLayout(hdr);

  // Current plan strip
  auto *cur = new QFrame(content);
  cur->setObjectName(QStringLiteral("cur"));
  cur->setStyleSheet(
      QStringLiteral("QFrame#cur{background:%1;border:1px solid %2;border-radius:12px;}QLabel{border:none;}")
          .arg(kCard, kLine)
  );
  auto *ch = new QHBoxLayout(cur);
  ch->setContentsMargins(16, 12, 16, 12);
  ch->setSpacing(14);
  auto *bolt = new QLabel(cur);
  bolt->setPixmap(tile(Icon::Bolt, 36, devicePixelRatioF(), kAccent, kBg));
  bolt->setStyleSheet(QStringLiteral("background:transparent;"));
  ch->addWidget(bolt);
  auto *ct = new QVBoxLayout();
  ct->setSpacing(2);
  m_currentTitle = text(QString(), kText, 14, 600, cur);
  m_currentDetail = text(QString(), kDim, 12, 400, cur);
  ct->addWidget(m_currentTitle);
  ct->addWidget(m_currentDetail);
  ch->addLayout(ct, 1);
  m_currentPill = text(QString(), kAccent, 11, 700, cur);
  m_currentPill->setStyleSheet(QStringLiteral(
                                   "color:%1;background:rgba(255,214,10,0.14);border-radius:9px;padding:3px 9px;"
                                   "font-size:11px;font-weight:700;"
  )
                                   .arg(kAccent));
  ch->addWidget(m_currentPill);
  col->addWidget(cur);

  // Two plan cards
  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(18);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);
  const QStringList perks{
      tr("Your main computer and one more"),
      tr("Mouse, keyboard, copy & paste, file drag"),
      tr("Encrypted, on your own Wi-Fi, no account"),
      tr("Up to 5 computers on one desk"),
      tr("Link computers on different networks"),
      tr("iPad and iPhone as extra screens"),
      tr("Priority support from a person")
  };
  QVBoxLayout *freeFoot = nullptr;
  QVBoxLayout *plusFoot = nullptr;
  m_freeCard =
      planCard(tr("Free"), QStringLiteral("$0"), tr("forever · 2 computers"), perks, 3, false, content, &freeFoot);
  m_plusCard = planCard(
      tr("Plus"), QStringLiteral("$24"), tr("/year · 1 desk · up to 5 computers"), perks, 7, true, content, &plusFoot
  );
  freeFoot->addWidget(text(tr("You're on Free. Nothing to do."), kDim, 12, 400, m_freeCard));
  m_plusButton = button(tr("Get Plus →"), true, m_plusCard);
  connect(m_plusButton, &QPushButton::clicked, this, &PlanView::getPlusRequested);
  plusFoot->addWidget(m_plusButton);
  plusFoot->addWidget(text(
      tr("14 days free when you first turn it on. Cancel any time; Free keeps working."), kDim, 11, 400, m_plusCard
  ));
  grid->addWidget(m_freeCard, 0, 0);
  grid->addWidget(m_plusCard, 0, 1);
  col->addLayout(grid);

  // Inline activation
  auto *actHead = new QHBoxLayout();
  auto *actLabel = text(tr("ALREADY HAVE A LICENSE?"), kDim, 11, 700, content);
  actLabel->setWordWrap(false);
  actHead->addWidget(actLabel);
  actHead->addStretch();
  col->addLayout(actHead);
  m_activatePanel = new QFrame(content);
  m_activatePanel->setObjectName(QStringLiteral("act"));
  m_activatePanel->setStyleSheet(QStringLiteral(
                                     "QFrame#act{background:%1;border:1px solid %2;border-radius:12px;}"
                                     "QLabel{border:none;}"
                                     "QPlainTextEdit{background:#000000;color:#FFFFFF;border:1px solid %2;"
                                     "border-radius:9px;padding:8px;font-family:ui-monospace,Menlo,Consolas,"
                                     "monospace;font-size:12px;}"
  )
                                     .arg(kCard, kLine));
  auto *av = new QVBoxLayout(m_activatePanel);
  av->setContentsMargins(16, 14, 16, 14);
  av->setSpacing(10);
  av->addWidget(text(
      tr("Paste the license from your email below. It's checked here on this computer; nothing is "
         "sent anywhere."),
      kMuted, 13, 400, m_activatePanel
  ));
  m_licenseText = new QPlainTextEdit(m_activatePanel);
  m_licenseText->setPlaceholderText(QStringLiteral("{ \"email\": \"you@example.com\", \"tier\": \"plus\", ... }"));
  m_licenseText->setFixedHeight(96);
  av->addWidget(m_licenseText);
  auto *arow = new QHBoxLayout();
  m_activateStatus = text(QString(), kDim, 12, 400, m_activatePanel);
  arow->addWidget(m_activateStatus, 1);
  m_activateButton = button(tr("Activate"), true, m_activatePanel);
  m_activateButton->setEnabled(false);
  arow->addWidget(m_activateButton);
  av->addLayout(arow);
  col->addWidget(m_activatePanel);
  connect(m_licenseText, &QPlainTextEdit::textChanged, this, [this] {
    const auto json = m_licenseText->toPlainText().trimmed().toUtf8();
    if (json.isEmpty()) {
      m_activateStatus->setText(QString());
      m_activateButton->setEnabled(false);
      return;
    }
    const auto lic = deskflow::License::fromJson(json);
    if (lic.isLicensed()) {
      m_activateStatus->setText(tr("✓ Valid %1 license for %2").arg(lic.tier(), lic.email()));
      m_activateStatus->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;").arg(kGreen));
      m_activateButton->setEnabled(true);
    } else {
      m_activateStatus->setText(
          lic.problem().isEmpty() ? tr("That doesn't look like a Lightning Desk license.") : lic.problem()
      );
      m_activateStatus->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;").arg(kRed));
      m_activateButton->setEnabled(false);
    }
  });
  connect(m_activateButton, &QPushButton::clicked, this, [this] {
    const auto json = m_licenseText->toPlainText().trimmed().toUtf8();
    QFile f(deskflow::License::licensePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      m_activateStatus->setText(tr("Couldn't save the license to %1.").arg(deskflow::License::licensePath()));
      m_activateStatus->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;").arg(kRed));
      return;
    }
    f.write(json);
    f.write("\n");
    f.close();
    m_licenseText->clear();
    deskflow::License::current(true);
    Q_EMIT licenseSaved();
  });

  col->addStretch(1);

  auto *scroll = new QScrollArea(this);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setWidget(content);
  scroll->setStyleSheet(QStringLiteral(
                            "QScrollArea{background:%1;border:none;}"
                            "QScrollBar:vertical{background:%1;width:8px;margin:0;}"
                            "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:24px;}"
                            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
  )
                            .arg(kBg, kLine));
  outer->addWidget(scroll);
}

void PlanView::reload(int computersOnDesk)
{
  const auto &lic = deskflow::License::current(true);
  const int extras = lic.maxExtraComputers();
  if (lic.isLicensed()) {
    const auto tier = lic.tier();
    m_currentTitle->setText(tr("Lightning Desk %1").arg(tier.left(1).toUpper() + tier.mid(1)));
    QString detail = tr("%1 · %2 of %n computers on your desk", "", extras + 1).arg(lic.email()).arg(computersOnDesk);
    if (lic.expires().isValid())
      detail += tr(" · until %1").arg(lic.expires().date().toString(Qt::ISODate));
    m_currentDetail->setText(detail);
    m_currentPill->setText(
        tier.compare(QStringLiteral("trial"), Qt::CaseInsensitive) == 0 ? tr("TRIAL") : tr("ACTIVE")
    );
    m_plusButton->setText(tr("Plus is on"));
    m_plusButton->setEnabled(false);
  } else {
    m_currentTitle->setText(tr("Lightning Desk Free"));
    m_currentDetail->setText(
        computersOnDesk > extras + 1
            ? tr("%1 on your desk · Free links %n at a time", "", extras + 1).arg(computersOnDesk)
            : tr("%1 of %n computers on your desk", "", extras + 1).arg(computersOnDesk)
    );
    m_currentPill->setText(tr("FREE"));
    m_plusButton->setText(tr("Get Plus →"));
    m_plusButton->setEnabled(true);
  }
}

} // namespace deskflow::gui
