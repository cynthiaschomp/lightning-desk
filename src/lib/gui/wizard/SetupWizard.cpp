// SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
// SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#include "SetupWizard.h"

#include "qrcodegen.hpp"

#include "common/Enums.h"
#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/TlsUtility.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include "net/Fingerprint.h"
#include "net/FingerprintDatabase.h"
#include "net/License.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QHostInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace deskflow::gui {

namespace {

const auto kAccent = QStringLiteral("#FFD60A");
const auto kBg = QStringLiteral("#000000");
const auto kCard = QStringLiteral("#1C1C1E");
const auto kCard2 = QStringLiteral("#2C2C2E");
const auto kLine = QStringLiteral("#3A3A3C");
const auto kText = QStringLiteral("#FFFFFF");
const auto kMuted = QStringLiteral("#AEAEB2");
const auto kDim = QStringLiteral("#A1A1A6");
const auto kGreen = QStringLiteral("#34C759");
const auto kRed = QStringLiteral("#FF453A");
const auto kPurple = QStringLiteral("#BF5AF2");

const int kConnectTimeoutMs = 25000;

const QString kStyle = QStringLiteral(R"(
QDialog { background: %1; }
QLabel { color: %5; }
QLabel[role="title"] { font-size: 24px; font-weight: 700; }
QLabel[role="body"] { font-size: 14px; color: %6; }
QLabel[role="eyebrow"] { font-size: 11px; font-weight: 600; color: %7; letter-spacing: 1px; }
QLabel[role="code"] { font-size: 40px; font-weight: 700; letter-spacing: 6px; }
QFrame[role="card"] { background: %2; border-radius: 14px; }
QLineEdit { background: transparent; border: none; color: %5; font-size: 16px; padding: 10px 0; }
QPlainTextEdit { background: %1; color: %6; border: 1px solid %8; border-radius: 8px; font-family: Consolas, monospace; font-size: 11px; }
QProgressBar { background: %3; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; }
QProgressBar::chunk { background: %4; border-radius: 3px; }
QPushButton[role="next"] { background: %4; color: #000000; border: none; border-radius: 12px; min-height: 46px; min-width: 200px; font-size: 16px; font-weight: 600; padding: 0 22px; }
QPushButton[role="next"]:hover { background: #FFE352; }
QPushButton[role="next"]:disabled { background: %3; color: %7; }
QPushButton[role="connect"] { background: %4; color: #000000; border: none; border-radius: 10px; min-height: 38px; font-size: 14px; font-weight: 600; padding: 0 18px; }
QPushButton[role="connect"]:disabled { background: %3; color: %7; }
QPushButton[role="back"] { background: transparent; color: %4; border: none; font-size: 14px; font-weight: 500; padding: 10px; }
QPushButton[role="link"] { background: transparent; color: %4; border: none; font-size: 13px; font-weight: 600; padding: 6px 0; text-align: left; }
QPushButton[role="choice"] { background: %2; color: %5; border: 2px solid %8; border-radius: 16px; padding: 18px; text-align: left; font-size: 15px; }
QPushButton[role="choice"]:checked { border-color: %4; }
QPushButton[role="choice"][variant="extra"]:checked { border-color: %9; }
QPushButton[role="side"] { background: %2; color: #E5E5EA; border: 2px solid %8; border-radius: 12px; font-size: 13px; font-weight: 600; }
QPushButton[role="side"]:checked { background: rgba(255,214,10,0.14); color: %4; border-color: %4; }
QPushButton[role="side"]:disabled { color: #48484A; border-color: %3; }
)")
                           .arg(kBg, kCard, kCard2, kAccent, kText, kMuted, kDim, kLine, kPurple);

/// Flat yellow lightning bolt drawn with QPainter (no emoji, no SVG module needed).
QPixmap boltPixmap(int size, qreal dpr)
{
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.scale(size / 24.0, size / 24.0);
  const QPolygonF bolt{{13, 2}, {4, 14}, {11, 14}, {10, 22}, {19, 10}, {12, 10}, {13, 2}};
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(kAccent));
  p.drawPolygon(bolt);
  return pm;
}

/// Simple monitor (main) or laptop (extra screen) outline in the given tint.
QPixmap devicePixmap(bool laptop, int size, qreal dpr, const QString &tint)
{
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.scale(size / 24.0, size / 24.0);
  QPen pen(QColor(tint), 2.0);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  if (laptop) {
    p.drawRoundedRect(QRectF(3, 4, 18, 12), 2, 2);
    p.drawLine(QPointF(2, 20), QPointF(22, 20));
  } else {
    p.drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
    p.drawLine(QPointF(8, 21), QPointF(16, 21));
    p.drawLine(QPointF(12, 17), QPointF(12, 21));
  }
  return pm;
}

/// Word-wrapped label at a fixed width, tall enough that no line is clipped.
void fitWidth(QLabel *label, int width)
{
  label->setFixedWidth(width);
  label->setMinimumHeight(label->heightForWidth(width));
}

QString dot(const QString &css)
{
  return QStringLiteral("min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px; border-radius: 5px; %1")
      .arg(css);
}

bool isPrivateV4(const QHostAddress &a)
{
  if (a.protocol() != QAbstractSocket::IPv4Protocol)
    return false;
  const quint32 ip = a.toIPv4Address();
  return (ip >> 24) == 10 || (ip >> 16) == 0xC0A8 || ((ip >> 20) == 0xAC1);
}

} // namespace

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

SetupWizard::SetupWizard(QWidget *parent)
    : QDialog(parent),
      m_serverConfig(std::make_unique<ServerConfig>()),
      m_core(std::make_unique<CoreProcess>(*m_serverConfig)),
      m_connectTimeout(new QTimer(this))
{
  setWindowTitle(tr("Lightning Desk"));
  setModal(true);
  setFixedSize(720, 640);
  setStyleSheet(kStyle);

  m_name = Settings::value(Settings::Core::ComputerName).toString();
  if (m_name.isEmpty())
    m_name = QHostInfo::localHostName().toUpper();

  m_connectTimeout->setSingleShot(true);
  connect(m_connectTimeout, &QTimer::timeout, this, [this] {
    if (m_awaitingAdmission) {
      m_awaitingAdmission = false;
      showExtraError(
          tr("%1 isn't ready for new computers").arg(m_mainName),
          tr("Open Lightning Desk on <b>%1</b> and go to the <b>Link your other computers</b> step, then try again.")
              .arg(m_mainName)
      );
    } else if (m_extraState == ExtraState::Connecting || m_extraState == ExtraState::Reconnecting) {
      showExtraError(
          tr("Couldn't connect to %1").arg(m_mainName),
          tr("We found it on your network, but it didn't answer within %1 seconds.").arg(kConnectTimeoutMs / 1000)
      );
    }
  });

  connect(m_core.get(), &CoreProcess::connectedClientsChanged, this, &SetupWizard::onConnectedClients);
  connect(m_core.get(), &CoreProcess::unrecognisedClient, this, &SetupWizard::onUnrecognisedClient);
  connect(m_core.get(), &CoreProcess::clientLimitReached, this, &SetupWizard::onClientLimitReached);
  connect(m_core.get(), &CoreProcess::peerFingerprint, this, &SetupWizard::onPeerFingerprint);
  connect(m_core.get(), &CoreProcess::logLine, this, &SetupWizard::onLogLine);
  connect(m_core.get(), &CoreProcess::connectionStateChanged, this, [this](deskflow::core::ConnectionState state) {
    if (m_role == Role::Main) {
      if (state == deskflow::core::ConnectionState::Listening || state == deskflow::core::ConnectionState::Connected)
        m_mainStatus->setText(QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp; %2")
                                  .arg(kGreen, tr("Ready — waiting for your other computers")));
      return;
    }
    if (state == deskflow::core::ConnectionState::Connected) {
      m_connectTimeout->stop();
      m_awaitingAdmission = false;
      setExtraStatus(
          ExtraState::Connected, tr("Connected to %1").arg(m_mainName),
          tr("This computer is now linked. Your mouse and keyboard will cross over as soon as the main computer "
             "finishes arranging your desk.")
      );
    } else if (state == deskflow::core::ConnectionState::Disconnected && m_extraState == ExtraState::Connected) {
      // The main computer briefly restarts its side when the desk changes; the
      // link comes back by itself within a second or two.
      setExtraStatus(
          ExtraState::Reconnecting, tr("Reconnecting to %1…").arg(m_mainName),
          tr("The main computer is updating its desk. This takes a moment and nothing needs doing.")
      );
      m_connectTimeout->start(kConnectTimeoutMs);
    }
    updateChrome();
  });
  connect(m_core.get(), &CoreProcess::connectionRefused, this, [this](deskflow::core::ConnectionRefusal reason) {
    if (m_role != Role::Extra)
      return;
    switch (reason) {
    case deskflow::core::ConnectionRefusal::UnknownClient:
      // First contact: the main computer has just been told about us and is
      // adding us to its desk. Our core retries every second; if the main
      // computer is on its Link step we'll be let in within a few seconds.
      if (!m_awaitingAdmission) {
        m_awaitingAdmission = true;
        setExtraStatus(
            ExtraState::Connecting, tr("%1 is adding this computer…").arg(m_mainName),
            tr("Almost there. The main computer is making room for this one.")
        );
        m_connectTimeout->start(kConnectTimeoutMs);
      }
      break;
    case deskflow::core::ConnectionRefusal::AlreadyConnected:
      showExtraError(
          tr("The main computer turned this one away"),
          tr("Either another computer called <b>%1</b> is already linked (go back a step and pick a different "
             "name), or the main computer is on the Free plan, which links it and one other computer. "
             "<a style='color:%2' href='%3'>Plus links up to five</a>.")
              .arg(m_name, kAccent, kUrlPlus)
      );
      break;
    case deskflow::core::ConnectionRefusal::IncompatibleVersion:
      showExtraError(
          tr("Versions don't match"), tr("Update Lightning Desk on both computers to the same version, then try again.")
      );
      break;
    case deskflow::core::ConnectionRefusal::ProtocolError:
      showExtraError(tr("Something went wrong while linking"), tr("The two computers couldn't agree on how to talk."));
      break;
    }
  });
  connect(m_core.get(), &CoreProcess::error, this, [this](CoreProcess::Error err) {
    const auto why = err == CoreProcess::Error::AddressMissing
                         ? tr("No desk code was entered.")
                         : tr("Lightning Desk couldn't start its connection on this computer.");
    if (m_role == Role::Extra) {
      showExtraError(tr("Couldn't start linking"), why);
    } else {
      m_mainStatus->setText(QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp; %2").arg(kRed, why));
    }
  });

  buildUi();
  goTo(Welcome);
}

SetupWizard::~SetupWizard()
{
  stopCore();
}

bool SetupWizard::isFirstRun()
{
  return !Settings::value(Settings::Gui::WizardCompleted).toBool();
}

void SetupWizard::openAtLink()
{
  m_linkOnly = true;
  goTo(Link);
  if (windowFlags().testFlag(Qt::Window))
    open();
  else
    show();
}

void SetupWizard::embedInto(QWidget *host)
{
  setParent(host, Qt::Widget);
  setModal(false);
  setMinimumSize(0, 0);
  setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SetupWizard::done(int result)
{
  // Hand the connection over to the main window cleanly.
  stopCore();
  QDialog::done(result);
}

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------

void SetupWizard::buildUi()
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(48, 28, 48, 20);
  root->setSpacing(0);

  auto *header = new QHBoxLayout();
  m_stepLabel = new QLabel(this);
  m_stepLabel->setProperty("role", "eyebrow");
  m_stepName = new QLabel(this);
  m_stepName->setProperty("role", "eyebrow");
  header->addWidget(m_stepLabel);
  header->addStretch();
  header->addWidget(m_stepName);
  root->addLayout(header);
  root->addSpacing(8);

  m_progress = new QProgressBar(this);
  m_progress->setRange(0, StepCount);
  m_progress->setTextVisible(false);
  root->addWidget(m_progress);
  root->addSpacing(24);

  m_pages = new QStackedWidget(this);
  m_pages->addWidget(buildWelcome());
  m_pages->addWidget(buildWhichComputer());
  m_pages->addWidget(buildName());
  m_pages->addWidget(buildLink());
  m_pages->addWidget(buildArrange());
  m_pages->addWidget(buildChecks());
  m_pages->addWidget(buildDone());
  root->addWidget(m_pages, 1);

  auto *footer = new QHBoxLayout();
  // Which build is this? Lets anyone check a machine runs the expected commit.
  auto *build = new QLabel(QStringLiteral("Lightning Desk %1").arg(QString::fromLatin1(kDisplayVersion)), this);
  build->setStyleSheet(QStringLiteral("font-size: 10px; color: #636366;"));
  build->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_back = new QPushButton(tr("Back"), this);
  m_back->setProperty("role", "back");
  m_back->setCursor(Qt::PointingHandCursor);
  m_skip = new QPushButton(tr("Skip for now"), this);
  m_skip->setProperty("role", "back");
  m_skip->setCursor(Qt::PointingHandCursor);
  m_next = new QPushButton(tr("Get started"), this);
  m_next->setProperty("role", "next");
  m_next->setCursor(Qt::PointingHandCursor);
  m_next->setDefault(true);
  footer->addWidget(m_back);
  footer->addStretch();
  footer->addWidget(m_skip);
  footer->addSpacing(8);
  footer->addWidget(m_next);
  root->addSpacing(16);
  root->addLayout(footer);
  root->addSpacing(18);
  root->addWidget(build, 0, Qt::AlignHCenter);

  connect(m_back, &QPushButton::clicked, this, &SetupWizard::back);
  connect(m_next, &QPushButton::clicked, this, &SetupWizard::next);
  connect(m_skip, &QPushButton::clicked, this, [this] {
    goTo(Checks);
    runChecks();
  });
}

QWidget *SetupWizard::card(QWidget *parent) const
{
  auto *f = new QFrame(parent);
  f->setProperty("role", "card");
  return f;
}

QLabel *SetupWizard::title(const QString &text, QWidget *parent) const
{
  auto *l = new QLabel(text, parent);
  l->setProperty("role", "title");
  l->setWordWrap(true);
  return l;
}

QLabel *SetupWizard::body(const QString &text, QWidget *parent) const
{
  auto *l = new QLabel(text, parent);
  l->setProperty("role", "body");
  l->setWordWrap(true);
  return l;
}

QPushButton *
SetupWizard::choiceButton(const QString &heading, const QString &detail, const QString &hint, bool extra) const
{
  auto *b = new QPushButton();
  b->setProperty("role", "choice");
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setMinimumHeight(196);
  // Qt ignores stylesheet padding for a button that owns a layout, so the
  // breathing room has to come from the layout itself.
  auto *lay = new QVBoxLayout(b);
  lay->setContentsMargins(22, 22, 22, 20);
  lay->setSpacing(10);

  const auto tint = extra ? kPurple : kAccent;
  auto *icon = new QLabel(b);
  icon->setFixedSize(48, 48);
  icon->setAlignment(Qt::AlignCenter);
  icon->setStyleSheet(
      QStringLiteral("background: %1; border-radius: 12px;")
          .arg(extra ? QStringLiteral("rgba(191,90,242,0.16)") : QStringLiteral("rgba(255,214,10,0.14)"))
  );
  icon->setPixmap(devicePixmap(extra, 26, b->devicePixelRatioF(), tint));

  auto *h = new QLabel(heading, b);
  h->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600; background: transparent;"));
  auto *d = new QLabel(detail, b);
  d->setWordWrap(true);
  d->setStyleSheet(QStringLiteral("font-size: 13px; color: %1; background: transparent; line-height: 145%;").arg(kMuted)
  );
  auto *t = new QLabel(hint, b);
  t->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: %1; background: transparent;").arg(tint));
  for (auto *l : {icon, h, d, t})
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
  lay->addWidget(icon);
  lay->addSpacing(2);
  lay->addWidget(h);
  lay->addWidget(d);
  lay->addStretch();
  lay->addWidget(t);
  return b;
}

QPushButton *SetupWizard::sideButton(const QString &text, Side side)
{
  auto *b = new QPushButton(text);
  b->setProperty("role", "side");
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  m_sideGroup->addButton(b, static_cast<int>(side));
  return b;
}

QWidget *SetupWizard::statusRow(const QString &dotStyle, const QString &name, const QString &state) const
{
  auto *row = new QFrame();
  row->setProperty("role", "card");
  auto *rl = new QHBoxLayout(row);
  rl->setContentsMargins(18, 12, 18, 12);
  rl->setSpacing(12);
  auto *d = new QLabel(row);
  d->setStyleSheet(dot(dotStyle));
  auto *n = new QLabel(name, row);
  n->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  auto *s = new QLabel(state, row);
  s->setStyleSheet(QStringLiteral("font-size: 12px; color: %1; background: transparent;").arg(kDim));
  rl->addWidget(d);
  rl->addWidget(n, 1);
  rl->addWidget(s);
  return row;
}

// ----------------------------------------------------------------------------
// Pages
// ----------------------------------------------------------------------------

QWidget *SetupWizard::buildWelcome()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  lay->setSpacing(16);

  auto *icon = new QLabel(w);
  icon->setFixedSize(96, 96);
  icon->setAlignment(Qt::AlignCenter);
  icon->setPixmap(boltPixmap(52, devicePixelRatioF()));
  icon->setStyleSheet(QStringLiteral("background: #0A0A0A; border: 1px solid %1; border-radius: 24px;").arg(kLine));
  lay->addWidget(icon, 0, Qt::AlignHCenter);

  auto *t = title(tr("One mouse. One keyboard.\nEvery computer on your desk."), w);
  t->setAlignment(Qt::AlignCenter);
  t->setStyleSheet(QStringLiteral("font-size: 28px;"));
  lay->addWidget(t);

  auto *b = body(
      tr("Move your cursor off the edge of one screen and it appears on the next. "
         "Copy on one computer, paste on another. Drag a file straight across. "
         "No cables, no extra gear."),
      w
  );
  b->setAlignment(Qt::AlignCenter);
  fitWidth(b, 500);
  lay->addWidget(b, 0, Qt::AlignHCenter);

  auto *pills = new QHBoxLayout();
  for (const auto &s : {tr("About 2 minutes"), tr("Nothing to type"), tr("Windows, Mac & Linux")}) {
    auto *p =
        new QLabel(QStringLiteral("<span style='color:%1'>&#10003;</span>&nbsp; %2").arg(kGreen, s.toHtmlEscaped()), w);
    p->setStyleSheet(
        QStringLiteral("background: %1; border-radius: 15px; padding: 7px 13px; font-size: 12px; color: #E5E5EA;")
            .arg(kCard)
    );
    pills->addWidget(p);
  }
  pills->setAlignment(Qt::AlignHCenter);
  lay->addLayout(pills);
  return w;
}

QWidget *SetupWizard::buildWhichComputer()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Which computer is this?"), w));
  lay->addWidget(body(tr("Pick the one that matches the computer you're sitting at right now."), w));
  lay->addSpacing(16);

  auto *row = new QHBoxLayout();
  row->setSpacing(16);
  m_mainChoice = choiceButton(
      tr("This is my main computer"), tr("The mouse and keyboard I want to use are plugged into this one."),
      tr("Most people start here")
  );
  m_extraChoice = choiceButton(
      tr("This is an extra screen"), tr("A laptop or second computer I want to control from my main one."),
      tr("Set up the main computer first"), true
  );
  m_extraChoice->setProperty("variant", "extra");
  m_mainChoice->setChecked(true);
  auto *group = new QButtonGroup(w);
  group->setExclusive(true);
  group->addButton(m_mainChoice, 0);
  group->addButton(m_extraChoice, 1);
  connect(group, &QButtonGroup::idClicked, this, [this](int id) { m_role = id == 0 ? Role::Main : Role::Extra; });
  row->addWidget(m_mainChoice);
  row->addWidget(m_extraChoice);
  lay->addLayout(row);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildName()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  lay->addWidget(title(tr("What should we call it?"), w));
  lay->addWidget(body(tr("This is the name you'll see when you arrange your desk. We filled in a suggestion."), w));
  lay->addSpacing(16);

  auto *c = card(w);
  auto *cl = new QHBoxLayout(c);
  cl->setContentsMargins(20, 8, 20, 8);
  auto *label = new QLabel(tr("Computer name"), c);
  label->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  label->setFixedWidth(130);
  m_nameEdit = new QLineEdit(m_name.toUpper(), c);
  m_nameEdit->setValidator(
      new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[a-zA-Z0-9._-]{1,63}")), m_nameEdit)
  );
  m_nameEdit->setStyleSheet(QStringLiteral("letter-spacing: 1px; font-weight: 600;"));
  // Names are always shown in capitals (they double as the desk code), so type them that way.
  connect(m_nameEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
    const auto upper = text.toUpper();
    if (upper == text)
      return;
    const int pos = m_nameEdit->cursorPosition();
    m_nameEdit->setText(upper);
    m_nameEdit->setCursorPosition(pos);
  });
  label->setBuddy(m_nameEdit);
  cl->addWidget(label);
  cl->addWidget(m_nameEdit, 1);
  lay->addWidget(c);

  auto *hint = body(tr("Keep it simple, like <b>DESK</b> or <b>SILVER-LAPTOP</b>. You can rename it anytime."), w);
  hint->setStyleSheet(
      QStringLiteral("background: rgba(255,214,10,0.10); border-radius: 12px; padding: 12px 14px; color: #F2D36B;")
  );
  lay->addSpacing(12);
  lay->addWidget(hint);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildLink()
{
  m_linkStack = new QStackedWidget(this);
  m_linkStack->addWidget(buildLinkMain());
  m_linkStack->addWidget(buildLinkExtra());
  return m_linkStack;
}

QWidget *SetupWizard::buildLinkMain()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Link your other computers"), w));
  lay->addWidget(body(
      tr("On each laptop, open Lightning Desk, choose <b>This is an extra screen</b>, and type this code. "
         "Tablets can scan the QR code."),
      w
  ));
  lay->addSpacing(10);

  auto *c = card(w);
  auto *cl = new QHBoxLayout(c);
  cl->setContentsMargins(22, 16, 16, 16);
  cl->setSpacing(18);
  auto *left = new QVBoxLayout();
  left->setSpacing(4);
  auto *eye = new QLabel(tr("YOUR DESK CODE"), c);
  eye->setProperty("role", "eyebrow");
  eye->setStyleSheet(QStringLiteral("background: transparent;"));
  // Phase 3 replaces this with a rotating code from the pairing service. Until
  // then the code is the main computer's name, which the extra screen looks up.
  m_codeLabel = new QLabel(c);
  m_codeLabel->setProperty("role", "code");
  m_codeLabel->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *sub = body(tr("Only works on your Wi‑Fi."), c);
  sub->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *trouble = new QPushButton(tr("Trouble linking?"), c);
  trouble->setProperty("role", "link");
  trouble->setCursor(Qt::PointingHandCursor);
  m_backupLabel = body(QString(), c);
  m_backupLabel->setStyleSheet(QStringLiteral("background: transparent; font-size: 12px;"));
  m_backupLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_backupLabel->hide();
  connect(trouble, &QPushButton::clicked, this, [this] { m_backupLabel->setVisible(!m_backupLabel->isVisible()); });
  left->addWidget(eye);
  left->addWidget(m_codeLabel);
  left->addWidget(sub);
  left->addWidget(trouble, 0, Qt::AlignLeft);
  left->addWidget(m_backupLabel);
  left->addStretch();
  cl->addLayout(left, 1);

  m_qrLabel = new QLabel(c);
  m_qrLabel->setFixedSize(132, 132);
  m_qrLabel->setAlignment(Qt::AlignCenter);
  m_qrLabel->setStyleSheet(QStringLiteral("background: #FFFFFF; border-radius: 12px;"));
  m_qrLabel->setToolTip(tr("Scan with a tablet to add it as a screen"));
  cl->addWidget(m_qrLabel, 0, Qt::AlignTop);
  lay->addWidget(c);

  lay->addSpacing(10);
  auto *listHead = new QHBoxLayout();
  auto *listEye = new QLabel(tr("LINKED SO FAR"), w);
  listEye->setProperty("role", "eyebrow");
  m_mainStatus = new QLabel(w);
  m_mainStatus->setOpenExternalLinks(true);
  m_mainStatus->setTextFormat(Qt::RichText);
  m_mainStatus->setStyleSheet(QStringLiteral("font-size: 11px; color: %1;").arg(kDim));
  listHead->addWidget(listEye);
  listHead->addStretch();
  listHead->addWidget(m_mainStatus);
  lay->addLayout(listHead);

  m_clientListLayout = new QVBoxLayout();
  m_clientListLayout->setSpacing(6);
  lay->addLayout(m_clientListLayout);

  m_addAnotherBtn = new QPushButton(tr("+  Add another computer"), w);
  m_addAnotherBtn->setProperty("role", "link");
  m_addAnotherBtn->setCursor(Qt::PointingHandCursor);
  connect(m_addAnotherBtn, &QPushButton::clicked, this, &SetupWizard::addAnother);
  lay->addWidget(m_addAnotherBtn, 0, Qt::AlignLeft);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildLinkExtra()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Connect to your main computer"), w));
  lay->addWidget(body(tr("Type the desk code shown on your main computer's screen."), w));
  lay->addSpacing(14);

  auto *c = card(w);
  auto *cl = new QHBoxLayout(c);
  cl->setContentsMargins(20, 10, 12, 10);
  auto *label = new QLabel(tr("Desk code"), c);
  label->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  label->setFixedWidth(100);
  m_extraCodeEdit = new QLineEdit(c);
  m_extraCodeEdit->setPlaceholderText(tr("For example CYN-01"));
  label->setBuddy(m_extraCodeEdit);
  m_connectBtn = new QPushButton(tr("Connect"), c);
  m_connectBtn->setProperty("role", "connect");
  m_connectBtn->setCursor(Qt::PointingHandCursor);
  cl->addWidget(label);
  cl->addWidget(m_extraCodeEdit, 1);
  cl->addWidget(m_connectBtn);
  lay->addWidget(c);
  connect(m_connectBtn, &QPushButton::clicked, this, &SetupWizard::connectToMain);
  connect(m_extraCodeEdit, &QLineEdit::returnPressed, this, &SetupWizard::connectToMain);

  lay->addSpacing(12);
  auto *status = card(w);
  m_extraStatusCard = status;
  auto *sl = new QVBoxLayout(status);
  sl->setContentsMargins(18, 14, 18, 14);
  sl->setSpacing(8);
  auto *top = new QHBoxLayout();
  top->setSpacing(12);
  m_extraStatusDot = new QLabel(status);
  m_extraStatusDot->setFixedSize(26, 26);
  m_extraStatusDot->setAlignment(Qt::AlignCenter);
  m_extraStatusTitle = new QLabel(status);
  m_extraStatusTitle->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600; background: transparent;"));
  top->addWidget(m_extraStatusDot);
  top->addWidget(m_extraStatusTitle, 1);
  sl->addLayout(top);
  m_extraStatusDetail = body(QString(), status);
  m_extraStatusDetail->setStyleSheet(QStringLiteral("background: transparent;"));
  m_extraStatusDetail->setTextFormat(Qt::RichText);
  m_extraStatusDetail->setOpenExternalLinks(true);
  sl->addWidget(m_extraStatusDetail);
  m_extraDetails = new QPlainTextEdit(status);
  m_extraDetails->setReadOnly(true);
  m_extraDetails->setFixedHeight(84);
  sl->addWidget(m_extraDetails);
  m_copyDetailsBtn = new QPushButton(tr("Copy details for support"), status);
  m_copyDetailsBtn->setProperty("role", "link");
  m_copyDetailsBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copyDetailsBtn, &QPushButton::clicked, this, [this] {
    QApplication::clipboard()->setText(
        m_extraStatusTitle->text() + QStringLiteral("\n\n") + m_extraDetails->toPlainText()
    );
    m_copyDetailsBtn->setText(tr("Copied"));
  });
  sl->addWidget(m_copyDetailsBtn, 0, Qt::AlignLeft);
  lay->addWidget(status);
  status->hide();
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildArrange()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  m_arrangeTitle = title(QString(), w);
  m_arrangeTitle->setTextFormat(Qt::RichText);
  lay->addWidget(m_arrangeTitle);
  m_arrangeIntro = body(QString(), w);
  lay->addWidget(m_arrangeIntro);
  lay->addSpacing(12);

  m_sideGroup = new QButtonGroup(w);
  m_sideGroup->setExclusive(true);

  auto *grid = new QGridLayout();
  grid->setSpacing(10);
  grid->setAlignment(Qt::AlignHCenter);
  auto *above = sideButton(tr("Above"), Side::Above);
  auto *left = sideButton(tr("Left"), Side::Left);
  auto *right = sideButton(tr("Right"), Side::Right);
  auto *below = sideButton(tr("Below"), Side::Below);
  above->setFixedSize(220, 60);
  below->setFixedSize(220, 60);
  left->setFixedSize(150, 124);
  right->setFixedSize(150, 124);

  m_mainTile = new QLabel(w);
  m_mainTile->setFixedSize(220, 124);
  m_mainTile->setAlignment(Qt::AlignCenter);
  m_mainTile->setStyleSheet(
      QStringLiteral(
          "background: %1; border: 1px solid #48484A; border-radius: 16px; font-size: 14px; font-weight: 600;"
      )
          .arg(kCard2)
  );

  grid->addWidget(above, 0, 1);
  grid->addWidget(left, 1, 0);
  grid->addWidget(m_mainTile, 1, 1);
  grid->addWidget(right, 1, 2);
  grid->addWidget(below, 2, 1);
  lay->addLayout(grid);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildChecks()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Making sure everything works"), w));
  lay->addWidget(body(tr("We check the things that usually go wrong and fix them for you. Nothing to do here."), w));
  lay->addSpacing(12);
  auto *c = card(w);
  m_checksLayout = new QVBoxLayout(c);
  m_checksLayout->setContentsMargins(0, 4, 0, 4);
  m_checksLayout->setSpacing(0);
  lay->addWidget(c);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildDone()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  lay->setSpacing(14);
  auto *check = new QLabel(QStringLiteral("<span style='font-size:34px;color:#000'>&#10003;</span>"), w);
  check->setFixedSize(72, 72);
  check->setAlignment(Qt::AlignCenter);
  check->setStyleSheet(QStringLiteral("background: %1; border-radius: 36px;").arg(kGreen));
  lay->addWidget(check, 0, Qt::AlignHCenter);
  auto *t = title(tr("You're all set"), w);
  t->setAlignment(Qt::AlignCenter);
  lay->addWidget(t);
  m_doneHint = body(QString(), w);
  m_doneHint->setTextFormat(Qt::RichText);
  m_doneHint->setAlignment(Qt::AlignCenter);
  m_doneHint->setFixedWidth(500);
  lay->addWidget(m_doneHint, 0, Qt::AlignHCenter);
  auto *feats = body(tr("Copy and paste works across them  ·  Drag files across too"), w);
  feats->setTextFormat(Qt::PlainText);
  feats->setAlignment(Qt::AlignCenter);
  fitWidth(feats, 500);
  lay->addSpacing(8);
  lay->addWidget(feats, 0, Qt::AlignHCenter);
  return w;
}

// ----------------------------------------------------------------------------
// Flow
// ----------------------------------------------------------------------------

void SetupWizard::goTo(int step)
{
  if (step < 0 || step >= StepCount)
    return;
  m_pages->setCurrentIndex(step);
  if (step == Link)
    enterLink();
  updateChrome();
}

void SetupWizard::updateChrome()
{
  const int step = m_pages->currentIndex();
  const QStringList names = {tr("Welcome"),      tr("Your role"), tr("Name"), tr("Link computers"),
                             tr("Arrange desk"), tr("Checks"),    tr("Done")};
  const QStringList nextText = {tr("Get started"), tr("Continue"), tr("Continue"), tr("I've linked them"),
                                tr("Continue"),    tr("Continue"), tr("Finish")};
  m_stepLabel->setText(tr("STEP %1 OF %2").arg(step + 1).arg(StepCount));
  m_stepName->setText(names.value(step).toUpper());
  m_progress->setValue(step + 1);
  m_next->setText(nextText.value(step));
  m_back->setVisible(step > Welcome);
  m_skip->setVisible(step == Link && m_role == Role::Main && m_everLinked.isEmpty());

  bool canGo = true;
  if (step == Link) {
    if (m_role == Role::Main) {
      canGo = !m_everLinked.isEmpty();
    } else {
      canGo = m_extraState == ExtraState::Connected;
      m_next->setText(m_extraState == ExtraState::Reconnecting ? tr("Reconnecting…") : tr("Continue"));
    }
  }
  m_next->setEnabled(canGo);

  if (step == Done) {
    QString hint;
    if (m_role == Role::Extra) {
      hint = tr("This computer is linked to <b>%1</b>. Slide the mouse off the edge of %1's screen to reach it.")
                 .arg(m_mainName.toHtmlEscaped());
    } else if (m_everLinked.isEmpty()) {
      hint = tr("When you're ready, open Lightning Desk on a laptop and choose <b>This is an extra screen</b>.");
    } else {
      QStringList lines;
      for (const auto &client : std::as_const(m_everLinked)) {
        if (!m_sideFor.contains(client))
          continue;
        lines << tr("Slide your mouse off the <b>%1</b> edge to reach <b>%2</b>.")
                     .arg(sideWord(m_sideFor.value(client)), client.toHtmlEscaped());
      }
      hint = tr("Try it now:") + QStringLiteral("<br>") + lines.join(QStringLiteral("<br>"));
    }
    m_doneHint->setText(hint);
    fitWidth(m_doneHint, 500);
  }
}

void SetupWizard::next()
{
  const int step = m_pages->currentIndex();
  applyStep(step);

  switch (step) {
  case Link:
    if (m_role == Role::Main && !m_everLinked.isEmpty()) {
      m_arrangeIndex = 0;
      goTo(Arrange);
      loadArrangeFor(0);
    } else {
      goTo(Checks);
      runChecks();
    }
    return;

  case Arrange: {
    const auto client = m_everLinked.value(m_arrangeIndex);
    const int id = m_sideGroup->checkedId();
    if (id < 0)
      return;
    const auto side = static_cast<Side>(id);
    m_sideFor.insert(client, side);
    m_serverConfig->placeClient(client, cellFor(side));
    m_serverConfig->commit();
    const int last = static_cast<int>(std::min<qsizetype>(m_everLinked.size(), 4)) - 1;
    if (m_arrangeIndex < last) {
      loadArrangeFor(m_arrangeIndex + 1);
      return;
    }
    applyServerConfig();
    goTo(Checks);
    runChecks();
    return;
  }

  case Done:
    Settings::setValue(Settings::Gui::WizardCompleted, true);
    Q_EMIT finishedSetup(m_role);
    accept();
    return;

  default:
    goTo(step + 1);
    return;
  }
}

void SetupWizard::back()
{
  const int step = m_pages->currentIndex();
  if (m_linkOnly && step == Link) {
    reject();
    return;
  }
  if (step == Arrange && m_arrangeIndex > 0) {
    loadArrangeFor(m_arrangeIndex - 1);
    return;
  }
  if (step == Checks) {
    if (m_role == Role::Main && !m_everLinked.isEmpty()) {
      goTo(Arrange);
      loadArrangeFor(static_cast<int>(std::min<qsizetype>(m_everLinked.size(), 4)) - 1);
    } else {
      goTo(Link);
    }
    return;
  }
  goTo(step - 1);
}

void SetupWizard::applyStep(int step)
{
  switch (step) {
  case WhichComputer:
    Settings::setValue(
        Settings::Core::CoreMode, m_role == Role::Main ? Settings::CoreMode::Server : Settings::CoreMode::Client
    );
    break;
  case Name:
    m_name = m_nameEdit->text().trimmed().toUpper();
    if (m_name.isEmpty())
      m_name = QHostInfo::localHostName().toUpper();
    Settings::setValue(Settings::Core::ComputerName, m_name);
    break;
  default:
    break;
  }
}

void SetupWizard::enterLink()
{
  if (m_role == Role::Main) {
    m_linkStack->setCurrentIndex(0);
    m_codeLabel->setText(m_name.toUpper());
    const auto addr = lanAddress();
    m_backupLabel->setText(
        addr.isEmpty() ? tr("No network found. Check this computer is on Wi‑Fi.")
                       : tr("If the code doesn't work, type <b>%1</b> on the laptop instead.").arg(addr)
    );
    m_qrLabel->setPixmap(pairingQr());
    if (!(m_core->isStarted() && m_core->mode() == Settings::CoreMode::Server)) {
      m_mainStatus->setText(tr("Starting…"));
      startCore(true);
    }
    refreshClientList();
  } else {
    m_linkStack->setCurrentIndex(1);
    m_extraCodeEdit->setFocus();
  }
}

// ----------------------------------------------------------------------------
// Core process
// ----------------------------------------------------------------------------

void SetupWizard::prepareMachine()
{
  // The Windows service cannot read the per-user settings file (it never
  // starts the core), so always run the core on the desktop.
#if defined(Q_OS_WIN)
  if (Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>() != Settings::ProcessMode::Desktop) {
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    m_fixedMode = true;
  }
#endif
  if (TlsUtility::isEnabled() && !TlsUtility::isCertValid())
    m_fixedCert = TlsUtility::generateCertificate();
  Settings::setValue(Settings::Server::ExternalConfig, false);
}

void SetupWizard::startCore(bool asMain)
{
  prepareMachine();
  const auto mode = asMain ? Settings::CoreMode::Server : Settings::CoreMode::Client;
  Settings::setValue(Settings::Core::CoreMode, mode);
  if (m_core->isStarted())
    m_core->stop();
  m_core->setMode(mode);
  m_core->start(Settings::ProcessMode::Desktop);
}

void SetupWizard::stopCore()
{
  if (m_connectTimeout)
    m_connectTimeout->stop();
  if (m_core && m_core->isStarted())
    m_core->stop();
}

void SetupWizard::onConnectedClients(const QStringList &clients)
{
  m_connected.clear();
  for (const auto &c : clients) {
    if (c == m_name)
      continue;
    m_connected << c;
    m_pending.removeAll(c);
    if (!m_everLinked.contains(c)) {
      m_everLinked << c;
      m_waiting = false;
    }
  }
  refreshClientList();
  updateChrome();
}

void SetupWizard::onClientLimitReached(const QString &name)
{
  if (m_role != Role::Main)
    return;
  const auto &license = deskflow::License::current(true);
  m_mainStatus->setText(QStringLiteral("<span style='color:%1'>&#9679;</span>&nbsp; %2")
                            .arg(
                                kAccent,
                                tr("%1 wants to join, but %2 links this computer and %n other(s). "
                                   "<a style='color:%3' href='%4'>Plus links up to five</a>.",
                                   "", license.maxExtraComputers())
                                    .arg(name, license.isLicensed() ? license.tier() : tr("Free"), kAccent, kUrlPlus)
                            ));
}

void SetupWizard::onUnrecognisedClient(const QString &name)
{
  if (m_role != Role::Main || m_pages->currentIndex() != Link)
    return;
  if (name.isEmpty() || name == m_name || m_serverConfig->isFull() || m_serverConfig->computerExists(name))
    return;
  // A new computer typed our code: place it on a free side so it can connect.
  // The Arrange step moves it to where it really sits.
  m_serverConfig->placeClient(name, nextFreeCell());
  m_serverConfig->commit();
  m_pending << name;
  refreshClientList();
  applyServerConfig();
}

void SetupWizard::applyServerConfig()
{
  // Prefer a live reload: the server keeps every linked computer connected.
  // Fall back to a restart only when the core can't be reached (clients then
  // reconnect by themselves within a second).
  if (!m_core->isStarted())
    return;
  if (!m_core->reloadServerConfig())
    m_core->restart();
}

void SetupWizard::onPeerFingerprint(const QString &fingerprint)
{
  // Pairing window: while the Link step is open, the computer the person just
  // linked on purpose is trusted on first use. The Phase 3 pairing service
  // replaces this with a code-verified exchange.
  if (m_pages->currentIndex() != Link)
    return;
  const auto hex = QString(fingerprint).remove(QLatin1Char(':'));
  if (m_trusted.contains(hex))
    return;
  m_trusted << hex;

  const Fingerprint sha256 = {QCryptographicHash::Sha256, QByteArray::fromHex(hex.toLatin1())};
  const bool asExtra = m_role == Role::Extra;
  const auto dbPath = asExtra ? Settings::tlsTrustedServersDb() : Settings::tlsTrustedClientsDb();
  FingerprintDatabase db;
  db.read(dbPath);
  if (db.isTrusted(sha256))
    return;
  db.addTrusted(sha256);
  if (!db.write(dbPath)) {
    if (asExtra)
      showExtraError(
          tr("Couldn't save the secure link"), tr("This computer couldn't write its list of trusted computers.")
      );
    return;
  }
  if (asExtra)
    m_core->restart();
}

void SetupWizard::onLogLine(const QString &line)
{
  const auto trimmed = line.trimmed();
  if (trimmed.isEmpty())
    return;
  m_recentLog << trimmed;
  while (m_recentLog.size() > 40)
    m_recentLog.removeFirst();
}

// ----------------------------------------------------------------------------
// Main computer: link list + QR
// ----------------------------------------------------------------------------

void SetupWizard::refreshClientList()
{
  while (auto *item = m_clientListLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  const auto green = QStringLiteral("background: %1;").arg(kGreen);
  const auto amber = QStringLiteral("background: %1;").arg(kAccent);
  const auto grey = QStringLiteral("border: 2px dashed #48484A;");

  for (const auto &c : std::as_const(m_everLinked)) {
    const bool live = m_connected.contains(c);
    m_clientListLayout->addWidget(statusRow(live ? green : amber, c, live ? tr("Linked") : tr("Reconnecting…")));
  }
  for (const auto &c : std::as_const(m_pending)) {
    if (!m_everLinked.contains(c))
      m_clientListLayout->addWidget(statusRow(amber, c, tr("Connecting…")));
  }
  if (m_waiting || (m_everLinked.isEmpty() && m_pending.isEmpty())) {
    auto *row = statusRow(grey, tr("Waiting for the next computer…"), QString());
    row->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(kDim));
    m_clientListLayout->addWidget(row);
  }

  m_addAnotherBtn->setVisible(!m_waiting && !m_everLinked.isEmpty());
}

void SetupWizard::addAnother()
{
  m_waiting = true;
  refreshClientList();
}

int SetupWizard::nextFreeCell() const
{
  // 5 x 3 grid, main computer in the centre (index 7).
  static const int order[] = {8, 6, 2, 12, 9, 5, 3, 1, 13, 11, 0, 4, 10, 14};
  const ServerConfig &config = *m_serverConfig;
  const auto &computers = config.computers();
  for (int i : order) {
    if (i < computers.size() && computers[i].isNull())
      return i;
  }
  return 8;
}

QString SetupWizard::lanAddress() const
{
  static const QStringList skip = {QStringLiteral("vethernet"), QStringLiteral("wsl"),
                                   QStringLiteral("hyper-v"),   QStringLiteral("nordlynx"),
                                   QStringLiteral("virtual"),   QStringLiteral("vmware"),
                                   QStringLiteral("docker"),    QStringLiteral("tailscale"),
                                   QStringLiteral("zerotier")};
  QString fallback;
  const auto interfaces = QNetworkInterface::allInterfaces();
  for (const auto &iface : interfaces) {
    const auto flags = iface.flags();
    if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
        (flags & QNetworkInterface::IsLoopBack))
      continue;
    const auto label = iface.humanReadableName().toLower();
    bool virtualIface = false;
    for (const auto &s : skip)
      virtualIface = virtualIface || label.contains(s);
    if (virtualIface)
      continue;
    const auto entries = iface.addressEntries();
    for (const auto &entry : entries) {
      const auto ip = entry.ip();
      if (!isPrivateV4(ip))
        continue;
      if (ip.toString().startsWith(QStringLiteral("192.168.")))
        return ip.toString();
      if (fallback.isEmpty())
        fallback = ip.toString();
    }
  }
  return fallback;
}

QPixmap SetupWizard::pairingQr() const
{
  const auto port = Settings::value(Settings::Core::Port).toInt();
  const auto fingerprint =
      TlsUtility::isEnabled() ? QString::fromLatin1(TlsUtility::certFingerprint().toHex()) : QString();
  const auto payload = QStringLiteral("lightningdesk://pair?v=1&name=%1&host=%2&port=%3&fp=%4")
                           .arg(
                               QString::fromLatin1(QUrl::toPercentEncoding(m_name)), lanAddress(),
                               QString::number(port > 0 ? port : 24800), fingerprint
                           );

  try {
    const auto qr = qrcodegen::QrCode::encodeText(payload.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 2;
    const int modules = qr.getSize() + border * 2;
    const int scale = std::max(1, 120 / modules);
    QImage img(modules * scale, modules * scale, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y) {
      for (int x = 0; x < qr.getSize(); ++x) {
        if (qr.getModule(x, y))
          p.drawRect((x + border) * scale, (y + border) * scale, scale, scale);
      }
    }
    p.end();
    return QPixmap::fromImage(img);
  } catch (const std::exception &e) {
    qWarning() << "could not build pairing qr code:" << e.what();
    return {};
  }
}

// ----------------------------------------------------------------------------
// Extra screen: connect + status
// ----------------------------------------------------------------------------

void SetupWizard::connectToMain()
{
  const auto code = m_extraCodeEdit->text().trimmed();
  if (code.isEmpty()) {
    showExtraError(tr("Enter the desk code first"), tr("You'll find it on the main computer's Link step."));
    return;
  }
  m_mainName = code;
  m_recentLog.clear();
  m_awaitingAdmission = false;
  setExtraStatus(ExtraState::Connecting, tr("Looking for %1…").arg(code), tr("This usually takes a few seconds."));

  QHostAddress direct;
  if (direct.setAddress(code)) {
    resolveAndConnect(code, true);
    return;
  }
  resolveAndConnect(code.toLower(), false);
}

void SetupWizard::resolveAndConnect(const QString &host, bool triedLocal)
{
  QHostAddress direct;
  if (direct.setAddress(host)) {
    Settings::setValue(Settings::Client::RemoteHost, host);
    setExtraStatus(
        ExtraState::Connecting, tr("Connecting to %1…").arg(m_mainName), tr("Setting up a private, encrypted link.")
    );
    startCore(false);
    m_connectTimeout->start(kConnectTimeoutMs);
    return;
  }

  QHostInfo::lookupHost(host, this, [this, host, triedLocal](const QHostInfo &info) {
    QString found;
    const auto addresses = info.addresses();
    for (const auto &a : addresses) {
      if (a.protocol() == QAbstractSocket::IPv4Protocol) {
        found = a.toString();
        break;
      }
    }
    if (!found.isEmpty()) {
      resolveAndConnect(found, true);
      return;
    }
    if (!triedLocal && !host.endsWith(QStringLiteral(".local"))) {
      resolveAndConnect(host + QStringLiteral(".local"), true);
      return;
    }
    showExtraError(
        tr("Couldn't find %1 on your network").arg(m_mainName),
        tr("No computer with that desk code answered on this Wi‑Fi.")
    );
  });
}

void SetupWizard::setExtraStatus(ExtraState state, const QString &heading, const QString &detail)
{
  m_extraState = state;
  m_extraStatusCard->show();
  m_extraStatusTitle->setText(heading);
  m_extraStatusDetail->setText(detail);

  QString dotCss;
  QString glyph;
  switch (state) {
  case ExtraState::Connected:
    dotCss = QStringLiteral("background: %1; border-radius: 13px; color: #000; font-weight: 700;").arg(kGreen);
    glyph = QStringLiteral("&#10003;");
    break;
  case ExtraState::Failed:
    dotCss = QStringLiteral("background: %1; border-radius: 13px; color: #000; font-weight: 700;").arg(kRed);
    glyph = QStringLiteral("!");
    break;
  case ExtraState::Reconnecting:
    dotCss = QStringLiteral("border: 3px solid %1; border-radius: 13px;").arg(kAccent);
    break;
  default:
    dotCss = QStringLiteral("border: 3px solid %1; border-top-color: %2; border-radius: 13px;").arg(kLine, kAccent);
    break;
  }
  m_extraStatusDot->setStyleSheet(dotCss);
  m_extraStatusDot->setText(glyph);

  const bool failed = state == ExtraState::Failed;
  m_extraDetails->setVisible(failed);
  m_copyDetailsBtn->setVisible(failed);
  m_copyDetailsBtn->setText(tr("Copy details for support"));
  m_connectBtn->setEnabled(state != ExtraState::Connecting);
  m_connectBtn->setText(failed ? tr("Try again") : tr("Connect"));
  updateChrome();
}

void SetupWizard::showExtraError(const QString &heading, const QString &why)
{
  m_connectTimeout->stop();
  if (m_core->isStarted())
    m_core->stop();

  const auto steps =
      tr("<ul style='margin-left:-20px'>"
         "<li>On <b>%1</b>, open Lightning Desk and stay on the <b>Link your other computers</b> step.</li>"
         "<li>Make sure both computers are on the same Wi‑Fi.</li>"
         "<li>Check the desk code is typed exactly as shown.</li>"
         "<li>Still stuck? On the main computer click <b>Trouble linking?</b> and type the backup code here.</li>"
         "</ul>")
          .arg(m_mainName.isEmpty() ? tr("the main computer") : m_mainName.toHtmlEscaped());

  setExtraStatus(ExtraState::Failed, heading, why + steps);

  QStringList details;
  details << tr("Desk code: %1").arg(m_mainName) << tr("This computer: %1").arg(m_name)
          << tr("Address tried: %1").arg(Settings::value(Settings::Client::RemoteHost).toString());
  QStringList problems;
  for (const auto &l : std::as_const(m_recentLog)) {
    if (l.contains(QStringLiteral("ERROR")) || l.contains(QStringLiteral("WARNING")) ||
        l.contains(QStringLiteral("FATAL")))
      problems << l;
  }
  const auto &tail = problems.isEmpty() ? m_recentLog : problems;
  for (qsizetype i = std::max<qsizetype>(0, tail.size() - 8); i < tail.size(); ++i)
    details << tail.at(i);
  m_extraDetails->setPlainText(details.join(QLatin1Char('\n')));
}

// ----------------------------------------------------------------------------
// Arrange
// ----------------------------------------------------------------------------

void SetupWizard::loadArrangeFor(int index)
{
  m_arrangeIndex = index;
  const auto client = m_everLinked.value(index);
  const int total = static_cast<int>(std::min<qsizetype>(m_everLinked.size(), 4));
  m_arrangeTitle->setText(tr("Where does <b>%1</b> sit?").arg(client.toHtmlEscaped()));
  m_arrangeIntro->setText(
      total > 1
          ? tr("Computer %1 of %2. Tap the side of your main screen where it physically is.").arg(index + 1).arg(total)
          : tr("Tap the side of your main screen where it physically is. Your cursor will cross over on that edge.")
  );
  m_mainTile->setText(QStringLiteral("%1<br><span style='font-size:11px;color:%2;font-weight:400'>%3</span>")
                          .arg(m_name.toHtmlEscaped(), kDim, tr("Main computer")));

  // Sides already given to earlier computers are taken.
  QList<Side> taken;
  for (int i = 0; i < index; ++i)
    taken << m_sideFor.value(m_everLinked.value(i));

  m_sideGroup->setExclusive(false);
  for (auto *b : m_sideGroup->buttons())
    b->setChecked(false);
  m_sideGroup->setExclusive(true);

  const auto current = m_sideFor.contains(client) ? m_sideFor.value(client) : Side::Right;
  bool picked = false;
  for (auto *b : m_sideGroup->buttons()) {
    const auto side = static_cast<Side>(m_sideGroup->id(b));
    b->setEnabled(!taken.contains(side));
    if (!picked && b->isEnabled() && side == current) {
      b->setChecked(true);
      picked = true;
    }
  }
  if (!picked) {
    for (auto *b : m_sideGroup->buttons()) {
      if (b->isEnabled()) {
        b->setChecked(true);
        break;
      }
    }
  }
}

int SetupWizard::cellFor(Side side) const
{
  switch (side) {
  case Side::Above:
    return 2;
  case Side::Left:
    return 6;
  case Side::Right:
    return 8;
  case Side::Below:
    return 12;
  }
  return 8;
}

QString SetupWizard::sideWord(Side side) const
{
  switch (side) {
  case Side::Above:
    return tr("top");
  case Side::Left:
    return tr("left");
  case Side::Right:
    return tr("right");
  case Side::Below:
    return tr("bottom");
  }
  return tr("right");
}

// ----------------------------------------------------------------------------
// Checks
// ----------------------------------------------------------------------------

void SetupWizard::addCheckRow(const QString &text, const QString &sub, bool fixed)
{
  auto *row = new QWidget();
  auto *rl = new QHBoxLayout(row);
  rl->setContentsMargins(18, 12, 18, 12);
  rl->setSpacing(14);
  auto *tick = new QLabel(QStringLiteral("<span style='color:#000;font-weight:700'>&#10003;</span>"), row);
  tick->setFixedSize(26, 26);
  tick->setAlignment(Qt::AlignCenter);
  tick->setStyleSheet(QStringLiteral("background: %1; border-radius: 13px;").arg(kGreen));
  auto *txt = new QLabel(
      sub.isEmpty() ? text
                    : QStringLiteral("%1<br><span style='font-size:11px;color:%2'>%3</span>").arg(text, kDim, sub),
      row
  );
  txt->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  rl->addWidget(tick);
  rl->addWidget(txt, 1);
  if (fixed) {
    auto *badge = new QLabel(tr("Fixed for you"), row);
    badge->setStyleSheet(QStringLiteral(
                             "font-size: 11px; font-weight: 600; color: %1; background: rgba(255,214,10,0.14); "
                             "border-radius: 10px; padding: 4px 9px;"
    )
                             .arg(kAccent));
    rl->addWidget(badge);
  }
  m_checksLayout->addWidget(row);
}

void SetupWizard::runChecks()
{
  while (auto *item = m_checksLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  prepareMachine();

  if (m_role == Role::Extra) {
    addCheckRow(tr("Linked to %1").arg(m_mainName), tr("Private and encrypted"), false);
  } else {
    const auto n = m_everLinked.size();
    addCheckRow(
        n == 0 ? tr("Ready for other computers") : tr("%n computer(s) linked", nullptr, static_cast<int>(n)),
        tr("Private and encrypted"), m_fixedCert
    );
  }
  addCheckRow(tr("Runs on your desktop, not as a hidden service"), QString(), m_fixedMode);
  addCheckRow(tr("Firewall lets your computers talk"), tr("Rule limited to your own network"), false);

  const bool autoStart = Settings::value(Settings::Gui::AutoStartCore).toBool();
  if (!autoStart)
    Settings::setValue(Settings::Gui::AutoStartCore, true);
  addCheckRow(tr("Starts by itself when you open Lightning Desk"), QString(), !autoStart);

  const bool clipboard = Settings::value(Settings::Server::EnableClipboard).toBool();
  if (!clipboard)
    Settings::setValue(Settings::Server::EnableClipboard, true);
  addCheckRow(tr("Copy, paste and drag files across computers"), QString(), !clipboard);
}

} // namespace deskflow::gui
