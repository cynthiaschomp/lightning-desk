/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/AboutView.h"

#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "desk/BrandIcons.h"

#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSysInfo>
#include <QUrl>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;

namespace deskflow::gui {

namespace {

QLabel *text(const QString &t, const QString &color, int px, int weight = 400, QWidget *parent = nullptr)
{
  auto *l = new QLabel(t, parent);
  l->setWordWrap(true);
  l->setTextInteractionFlags(Qt::TextBrowserInteraction);
  l->setOpenExternalLinks(true);
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
                       "QPushButton{background:%1;color:%2;border:none;border-radius:11px;padding:8px 16px;"
                       "font-size:13px;font-weight:600;}QPushButton:hover{background:%3;}"
  )
                       .arg(
                           primary ? kAccent : kCard2, primary ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF"),
                           primary ? QStringLiteral("#FFE14D") : kLine
                       ));
  return b;
}

/// A clickable row (title, subtitle, chevron) that opens a URL.
QWidget *linkRow(const QString &title, const QString &sub, const QString &url, bool first, bool last, QWidget *parent)
{
  auto *b = new QPushButton(parent);
  b->setCursor(Qt::PointingHandCursor);
  const QString radius = (first && last) ? QStringLiteral("border-radius:12px;")
                         : first         ? QStringLiteral("border-top-left-radius:12px;border-top-right-radius:12px;")
                         : last ? QStringLiteral("border-bottom-left-radius:12px;border-bottom-right-radius:12px;")
                                : QString();
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{background:%1;border:1px solid %2;%3%4text-align:left;padding:0;}"
                       "QPushButton:hover{background:%5;}"
  )
                       .arg(kCard, kLine, radius, last ? QString() : QStringLiteral("border-bottom:none;"), kCard2));
  auto *h = new QHBoxLayout(b);
  h->setContentsMargins(16, 11, 14, 11);
  h->setSpacing(12);
  auto *txt = new QVBoxLayout();
  txt->setSpacing(2);
  auto *t = text(title, kText, 13, 600, b);
  t->setWordWrap(false);
  t->setTextInteractionFlags(Qt::NoTextInteraction);
  t->setAttribute(Qt::WA_TransparentForMouseEvents);
  txt->addWidget(t);
  if (!sub.isEmpty()) {
    auto *s = text(sub, kDim, 11, 400, b);
    s->setWordWrap(false);
    s->setTextInteractionFlags(Qt::NoTextInteraction);
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    txt->addWidget(s);
  }
  h->addLayout(txt, 1);
  auto *chev = text(QStringLiteral("›"), kDim, 18, 400, b);
  chev->setAttribute(Qt::WA_TransparentForMouseEvents);
  h->addWidget(chev);
  // A QPushButton doesn't size itself from a child layout; give it the layout's height.
  b->setMinimumHeight(h->sizeHint().height());
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(b, &QPushButton::clicked, b, [url] { QDesktopServices::openUrl(QUrl(url)); });
  return b;
}

QWidget *group(const QString &title, const QList<QWidget *> &rows, QWidget *parent)
{
  auto *w = new QWidget(parent);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  auto *l = text(title.toUpper(), kDim, 11, 700, w);
  l->setContentsMargins(2, 0, 0, 8);
  v->addWidget(l);
  for (auto *r : rows)
    v->addWidget(r);
  return w;
}

} // namespace

AboutView::AboutView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("aboutView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#aboutView{background:%1;}").arg(kBg));

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);
  col->addWidget(text(tr("About"), kText, 22, 700, content));

  // Hero card
  auto *hero = new QFrame(content);
  hero->setObjectName(QStringLiteral("hero"));
  hero->setStyleSheet(QStringLiteral("QFrame#hero{background:%1;border-radius:18px;}QLabel{border:none;}").arg(kCard));
  auto *hh = new QHBoxLayout(hero);
  hh->setContentsMargins(22, 20, 22, 20);
  hh->setSpacing(20);
  auto *logo = new QLabel(hero);
  logo->setPixmap(tile(Icon::Bolt, 76, devicePixelRatioF(), QStringLiteral("#0A0A0A"), kAccent));
  logo->setStyleSheet(QStringLiteral("background:transparent;"));
  hh->addWidget(logo, 0, Qt::AlignTop);
  auto *ht = new QVBoxLayout();
  ht->setSpacing(4);
  ht->addWidget(text(QStringLiteral("Lightning Desk"), kText, 24, 700, hero));
  ht->addWidget(text(
      tr("Version %1 <span style='color:%2'>(%3)</span> · %4")
          .arg(kVersion, kDim, kVersionGitSha, QSysInfo::prettyProductName()),
      kMuted, 13, 400, hero
  ));
  m_updateLine = text(tr("Updates are checked when you turn them on in Settings."), kDim, 13, 400, hero);
  ht->addWidget(m_updateLine);
  hh->addLayout(ht, 1);
  m_updateButton = button(tr("Check for updates"), false, hero);
  connect(m_updateButton, &QPushButton::clicked, this, [this] {
    if (m_newest.isEmpty())
      Q_EMIT checkForUpdatesRequested();
    else
      Q_EMIT getUpdateRequested();
  });
  hh->addWidget(m_updateButton, 0, Qt::AlignTop);
  col->addWidget(hero);

  // Something not working? -> Checks page (inline, never a window)
  auto *trouble = new QPushButton(content);
  trouble->setCursor(Qt::PointingHandCursor);
  trouble->setStyleSheet(QStringLiteral(
                             "QPushButton{background:%1;border:1px solid %2;border-radius:12px;text-align:left;"
                             "padding:0;}QPushButton:hover{background:%3;}"
  )
                             .arg(kCard, kLine, kCard2));
  auto *th = new QHBoxLayout(trouble);
  th->setContentsMargins(16, 12, 14, 12);
  th->setSpacing(14);
  auto *ticon = new QLabel(trouble);
  ticon->setPixmap(tile(Icon::Help, 36, devicePixelRatioF(), kCard2, kAccent));
  ticon->setStyleSheet(QStringLiteral("background:transparent;"));
  ticon->setAttribute(Qt::WA_TransparentForMouseEvents);
  th->addWidget(ticon);
  auto *tt = new QVBoxLayout();
  tt->setSpacing(2);
  auto *t1 = text(tr("Something not working?"), kText, 13, 600, trouble);
  auto *t2 =
      text(tr("Run the checks. We fix what we can and show the rest in plain language."), kDim, 11, 400, trouble);
  for (auto *l : {t1, t2}) {
    l->setWordWrap(false);
    l->setTextInteractionFlags(Qt::NoTextInteraction);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    tt->addWidget(l);
  }
  th->addLayout(tt, 1);
  auto *tchev = text(QStringLiteral("›"), kDim, 18, 400, trouble);
  tchev->setAttribute(Qt::WA_TransparentForMouseEvents);
  th->addWidget(tchev);
  trouble->setMinimumHeight(th->sizeHint().height());
  trouble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(trouble, &QPushButton::clicked, this, &AboutView::checksRequested);
  col->addWidget(trouble);

  // Security code card
  auto *code = new QFrame(content);
  code->setObjectName(QStringLiteral("codeCard"));
  code->setStyleSheet(QStringLiteral(
                          "QFrame#codeCard{background:%1;border:1px solid %2;border-radius:12px;}"
                          "QLabel{border:none;}"
  )
                          .arg(kCard, kLine));
  auto *ch = new QHBoxLayout(code);
  ch->setContentsMargins(16, 12, 16, 12);
  ch->setSpacing(14);
  auto *lock = new QLabel(code);
  lock->setPixmap(tile(Icon::Lock, 36, devicePixelRatioF(), kCard2, kAccent));
  lock->setStyleSheet(QStringLiteral("background:transparent;"));
  ch->addWidget(lock);
  auto *ct = new QVBoxLayout();
  ct->setSpacing(2);
  ct->addWidget(text(tr("This computer's security code"), kText, 13, 600, code));
  ct->addWidget(text(
      tr("Other computers show it when this one asks to join. It only changes if you make a new key."), kDim, 11, 400,
      code
  ));
  ch->addLayout(ct, 1);
  m_code = new QLabel(code);
  m_code->setStyleSheet(QStringLiteral(
      "font-family:ui-monospace,Menlo,Consolas,monospace;font-size:16px;"
      "font-weight:600;letter-spacing:2px;color:#FFFFFF;background:#000000;"
      "border-radius:8px;padding:6px 10px;"
  ));
  ch->addWidget(m_code);
  col->addWidget(code);

  // Two columns
  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(24);
  grid->setVerticalSpacing(18);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);
  grid->addWidget(
      group(
          tr("Made by"),
          {linkRow(
               QStringLiteral("Schomp Technologies LLC"), kUrlSupportMail,
               QStringLiteral("mailto:%1?subject=Lightning%20Desk").arg(kUrlSupportMail), true, false, content
           ),
           linkRow(tr("Release notes"), tr("What changed in each version"), kUrlReleaseNotes, false, false, content),
           linkRow(
               tr("Report a problem"), tr("Copy diagnostics first, then paste them into the email"),
               QStringLiteral("mailto:%1?subject=Lightning%20Desk%20problem").arg(kUrlSupportMail), false, true, content
           )},
          content
      ),
      0, 0, Qt::AlignTop
  );
  grid->addWidget(
      group(
          tr("Credits & license"),
          {linkRow(
               tr("Based on Deskflow"), tr("© 2024–2026 Deskflow Developers and earlier Synergy contributors"),
               QStringLiteral("https://github.com/deskflow/deskflow"), true, false, content
           ),
           linkRow(
               tr("GNU GPL v2.0 with OpenSSL exception"), tr("You can view and change the source code"), kUrlApp, false,
               false, content
           ),
           linkRow(
               tr("Open-source components"), QStringLiteral("Qt, OpenSSL, libei, libportal, Nayuki QR"),
               QStringLiteral("%1/blob/main/LICENSE").arg(kUrlApp), false, true, content
           )},
          content
      ),
      0, 1, Qt::AlignTop
  );
  col->addLayout(grid);

  // Footer
  auto *foot = new QHBoxLayout();
  foot->setSpacing(10);
  auto *copyDiag = button(tr("Copy diagnostics"), false, content);
  auto *logs = button(tr("Show log folder"), false, content);
  connect(copyDiag, &QPushButton::clicked, this, &AboutView::copyDiagnosticsRequested);
  connect(logs, &QPushButton::clicked, this, &AboutView::openLogsRequested);
  foot->addWidget(copyDiag);
  foot->addWidget(logs);
  foot->addStretch();
  foot->addWidget(text(tr("© 2026 Schomp Technologies LLC"), kDim, 12, 400, content));
  col->addLayout(foot);
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

void AboutView::setSecurityCode(const QString &sha256Hex)
{
  const auto up = sha256Hex.toUpper();
  QStringList groups;
  for (int i = 0; i + 4 <= up.size() && groups.size() < 4; i += 4)
    groups << up.mid(i, 4);
  m_code->setText(groups.join(QStringLiteral("  ")));
  m_code->setVisible(!groups.isEmpty());
}

void AboutView::setChecking()
{
  m_updateLine->setText(tr("Checking…"));
  m_updateLine->setStyleSheet(m_updateLine->styleSheet().replace(kGreen, kDim).replace(kAccent, kDim));
  m_updateButton->setEnabled(false);
}

void AboutView::setUpdateState(bool ok, bool upToDate, const QString &newest)
{
  m_updateButton->setEnabled(true);
  if (!ok) {
    m_newest.clear();
    m_updateLine->setText(tr("Couldn't reach schomp.ai to check. Try again later."));
    m_updateLine->setStyleSheet(QStringLiteral("color:%1;font-size:13px;background:transparent;border:none;").arg(kDim)
    );
    m_updateButton->setText(tr("Check for updates"));
    return;
  }
  if (upToDate) {
    m_newest.clear();
    m_updateLine->setText(tr("✓ You're up to date"));
    m_updateLine->setStyleSheet(
        QStringLiteral("color:%1;font-size:13px;font-weight:600;background:transparent;border:none;").arg(kGreen)
    );
    m_updateButton->setText(tr("Check for updates"));
    return;
  }
  m_newest = newest;
  m_updateLine->setText(tr("Version %1 is ready. Nothing installs until you say so.").arg(newest));
  m_updateLine->setStyleSheet(
      QStringLiteral("color:%1;font-size:13px;font-weight:600;background:transparent;border:none;").arg(kAccent)
  );
  m_updateButton->setText(tr("Get %1").arg(newest));
}

void AboutView::setDiagnostics(const QString &)
{
}

} // namespace deskflow::gui
