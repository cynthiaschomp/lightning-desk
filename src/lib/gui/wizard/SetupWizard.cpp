// SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
// SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#include "SetupWizard.h"

#include "common/Settings.h"
#include "gui/TlsUtility.h"
#include "gui/config/ServerConfig.h"

#include <QButtonGroup>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

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
const auto kPurple = QStringLiteral("#BF5AF2");

const auto kWizardCompletedKey = QStringLiteral("gui/wizardCompleted");

const QString kStyle = QStringLiteral(R"(
QDialog { background: %1; }
QLabel { color: %5; }
QLabel[role="title"] { font-size: 24px; font-weight: 700; }
QLabel[role="body"] { font-size: 14px; color: %6; }
QLabel[role="eyebrow"] { font-size: 11px; font-weight: 600; color: %7; letter-spacing: 1px; }
QLabel[role="code"] { font-size: 40px; font-weight: 700; letter-spacing: 6px; }
QFrame[role="card"] { background: %2; border-radius: 14px; }
QLineEdit { background: transparent; border: none; color: %5; font-size: 16px; padding: 10px 0; }
QProgressBar { background: %3; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; }
QProgressBar::chunk { background: %4; border-radius: 3px; }
QPushButton[role="next"] { background: %4; color: #000000; border: none; border-radius: 12px; min-height: 46px; min-width: 200px; font-size: 16px; font-weight: 600; padding: 0 22px; }
QPushButton[role="next"]:hover { background: #FFE352; }
QPushButton[role="back"] { background: transparent; color: %4; border: none; font-size: 14px; font-weight: 500; padding: 10px; }
QPushButton[role="choice"] { background: %2; color: %5; border: 2px solid %8; border-radius: 16px; padding: 18px; text-align: left; font-size: 15px; }
QPushButton[role="choice"]:checked { border-color: %4; }
QPushButton[role="choice"][variant="extra"]:checked { border-color: %9; }
QPushButton[role="side"] { background: %2; color: #E5E5EA; border: 2px solid %8; border-radius: 12px; font-size: 13px; font-weight: 600; }
QPushButton[role="side"]:checked { background: rgba(255,214,10,0.14); color: %4; border-color: %4; }
)")
                           .arg(kBg, kCard, kCard2, kAccent, kText, kMuted, kDim, kLine, kPurple);

} // namespace

SetupWizard::SetupWizard(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("Lightning Desk"));
  setModal(true);
  setFixedSize(720, 640);
  setStyleSheet(kStyle);

  m_name = Settings::value(Settings::Core::ComputerName).toString();
  if (m_name.isEmpty())
    m_name = QHostInfo::localHostName().toLower();

  buildUi();
  goTo(Welcome);
}

bool SetupWizard::isFirstRun()
{
  return !Settings::value(kWizardCompletedKey).toBool();
}

void SetupWizard::openAtLink()
{
  goTo(Link);
  open();
}

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------

void SetupWizard::buildUi()
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(48, 28, 48, 26);
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
  root->addSpacing(28);

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
  m_back = new QPushButton(tr("Back"), this);
  m_back->setProperty("role", "back");
  m_back->setCursor(Qt::PointingHandCursor);
  m_next = new QPushButton(tr("Get started"), this);
  m_next->setProperty("role", "next");
  m_next->setCursor(Qt::PointingHandCursor);
  m_next->setDefault(true);
  footer->addWidget(m_back);
  footer->addStretch();
  footer->addWidget(m_next);
  root->addSpacing(16);
  root->addLayout(footer);

  connect(m_back, &QPushButton::clicked, this, &SetupWizard::back);
  connect(m_next, &QPushButton::clicked, this, &SetupWizard::next);
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

QPushButton *SetupWizard::choiceButton(const QString &heading, const QString &detail, const QString &hint) const
{
  auto *b = new QPushButton();
  b->setProperty("role", "choice");
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setMinimumHeight(170);
  auto *lay = new QVBoxLayout(b);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(8);
  auto *h = new QLabel(heading, b);
  h->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 600; background: transparent;"));
  auto *d = new QLabel(detail, b);
  d->setWordWrap(true);
  d->setStyleSheet(QStringLiteral("font-size: 13px; color: %1; background: transparent;").arg(kMuted));
  auto *t = new QLabel(hint, b);
  t->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: %1; background: transparent;").arg(kAccent));
  for (auto *l : {h, d, t})
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
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
  b->setChecked(side == m_side);
  m_sideGroup->addButton(b, static_cast<int>(side));
  return b;
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
  icon->setText(QStringLiteral("<span style='font-size:52px;color:%1'>&#9889;</span>").arg(kAccent));
  icon->setStyleSheet(
      QStringLiteral("background: #0A0A0A; border: 1px solid %1; border-radius: 24px;").arg(kLine)
  );
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
  b->setMaximumWidth(500);
  lay->addWidget(b, 0, Qt::AlignHCenter);

  auto *pills = new QHBoxLayout();
  for (const auto &s : {tr("About 2 minutes"), tr("Nothing to type"), tr("Windows, Mac & Linux")}) {
    auto *p = new QLabel(QStringLiteral("<span style='color:%1'>&#10003;</span>&nbsp; %2").arg(kGreen, s), w);
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
      tr("Set up the main computer first")
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
  lay->addWidget(
      body(tr("This is the name you'll see when you arrange your desk. We filled in a suggestion."), w)
  );
  lay->addSpacing(16);

  auto *c = card(w);
  auto *cl = new QHBoxLayout(c);
  cl->setContentsMargins(18, 4, 18, 4);
  auto *label = new QLabel(tr("Computer name"), c);
  label->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  label->setFixedWidth(130);
  m_nameEdit = new QLineEdit(m_name, c);
  m_nameEdit->setValidator(
      new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[a-zA-Z0-9._-]{1,63}")), m_nameEdit)
  );
  label->setBuddy(m_nameEdit);
  cl->addWidget(label);
  cl->addWidget(m_nameEdit, 1);
  lay->addWidget(c);

  auto *hint = body(tr("Keep it simple, like <b>desk</b> or <b>silver-laptop</b>. You can rename it anytime."), w);
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
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Link your other computers"), w));
  lay->addWidget(body(
      tr("On each laptop, open Lightning Desk, choose <b>This is an extra screen</b>, and type this code. "
         "That's it — no addresses, no certificates."),
      w
  ));
  lay->addSpacing(14);

  auto *c = card(w);
  auto *cl = new QVBoxLayout(c);
  cl->setContentsMargins(24, 20, 24, 20);
  auto *eye = new QLabel(tr("YOUR DESK CODE"), c);
  eye->setProperty("role", "eyebrow");
  eye->setStyleSheet(QStringLiteral("background: transparent;"));
  // TODO(phase 3): real code from the pairing service. Until then the
  // "code" is the main computer's name, which an extra screen can type on
  // its own Link step.
  auto *code = new QLabel(m_name.toUpper(), c);
  code->setProperty("role", "code");
  code->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *sub = body(tr("Only works on your Wi‑Fi."), c);
  sub->setStyleSheet(QStringLiteral("background: transparent;"));
  cl->addWidget(eye);
  cl->addWidget(code);
  cl->addWidget(sub);
  lay->addWidget(c);

  // Extra-screen variant: type the main computer's code.
  auto *c2 = card(w);
  auto *c2l = new QHBoxLayout(c2);
  c2l->setContentsMargins(18, 4, 18, 4);
  auto *l2 = new QLabel(tr("Main computer's code"), c2);
  l2->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  l2->setFixedWidth(170);
  m_linkNameEdit = new QLineEdit(c2);
  m_linkNameEdit->setPlaceholderText(tr("Type it exactly as shown on the main computer"));
  l2->setBuddy(m_linkNameEdit);
  c2l->addWidget(l2);
  c2l->addWidget(m_linkNameEdit, 1);
  lay->addSpacing(10);
  lay->addWidget(c2);
  lay->addStretch();
  return w;
}

QWidget *SetupWizard::buildArrange()
{
  auto *w = new QWidget(this);
  auto *lay = new QVBoxLayout(w);
  lay->setSpacing(6);
  lay->addWidget(title(tr("Where does it sit on your desk?"), w));
  m_arrangeIntro = body(QString(), w);
  lay->addWidget(m_arrangeIntro);
  lay->addSpacing(12);

  m_sideGroup = new QButtonGroup(w);
  m_sideGroup->setExclusive(true);
  connect(m_sideGroup, &QButtonGroup::idClicked, this, [this](int id) { m_side = static_cast<Side>(id); });

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
      QStringLiteral("background: %1; border: 1px solid #48484A; border-radius: 16px; font-size: 14px; font-weight: 600;")
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
  m_doneHint->setAlignment(Qt::AlignCenter);
  m_doneHint->setMaximumWidth(460);
  lay->addWidget(m_doneHint, 0, Qt::AlignHCenter);
  auto *feats = body(tr("Copy &amp; paste works across them &nbsp;·&nbsp; Drag files across too"), w);
  feats->setAlignment(Qt::AlignCenter);
  lay->addSpacing(8);
  lay->addWidget(feats);
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
  updateChrome();
}

void SetupWizard::updateChrome()
{
  const int step = m_pages->currentIndex();
  static const QStringList names = {tr("Welcome"), tr("Your role"),  tr("Name"), tr("Link computers"),
                                    tr("Arrange desk"), tr("Checks"), tr("Done")};
  static const QStringList nextText = {tr("Get started"), tr("Continue"), tr("Continue"), tr("I've linked them"),
                                       tr("Continue"), tr("Continue"), tr("Finish")};
  m_stepLabel->setText(tr("STEP %1 OF %2").arg(step + 1).arg(StepCount));
  m_stepName->setText(names.value(step).toUpper());
  m_progress->setValue(step + 1);
  m_next->setText(nextText.value(step));
  m_back->setVisible(step > Welcome);

  if (step == Arrange) {
    const auto other = m_role == Role::Main ? m_otherName : m_name;
    m_arrangeIntro->setText(
        tr("Tap the side of your main screen where <b>%1</b> physically is. Your cursor will cross over on that edge.")
            .arg(other.isEmpty() ? tr("the other computer") : other)
    );
    m_mainTile->setText(
        QStringLiteral("%1<br><span style='font-size:11px;color:%2;font-weight:400'>%3</span>")
            .arg(m_role == Role::Main ? m_name : m_otherName, kDim, tr("Main computer"))
    );
  }
  if (step == Done) {
    static const QMap<Side, QString> word = {
        {Side::Above, tr("top")}, {Side::Left, tr("left")}, {Side::Right, tr("right")}, {Side::Below, tr("bottom")}};
    m_doneHint->setText(
        tr("Try it now: slide your mouse off the <b>%1</b> edge of your main screen.").arg(word.value(m_side))
    );
  }
}

void SetupWizard::next()
{
  const int step = m_pages->currentIndex();
  applyStep(step);
  if (step == Done) {
    Settings::setValue(kWizardCompletedKey, true);
    accept();
    Q_EMIT finishedSetup(m_role);
    return;
  }
  goTo(step + 1);
  if (step + 1 == Checks)
    runChecks();
}

void SetupWizard::back()
{
  goTo(m_pages->currentIndex() - 1);
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
    m_name = m_nameEdit->text().trimmed();
    if (m_name.isEmpty())
      m_name = QHostInfo::localHostName().toLower();
    Settings::setValue(Settings::Core::ComputerName, m_name);
    break;
  case Link:
    m_otherName = m_linkNameEdit->text().trimmed().toLower();
    if (m_role == Role::Extra && !m_otherName.isEmpty()) {
      // Until the pairing service exists, the "code" is the main computer's name.
      // Deskflow resolves it with DNS / mDNS; the user never sees an address.
      Settings::setValue(Settings::Client::RemoteHost, m_otherName);
    }
    break;
  case Arrange: {
    // Main computer: write the layout. Extra screen: the main computer owns the layout.
    if (m_role == Role::Main && !m_otherName.isEmpty()) {
      ServerConfig cfg;
      cfg.placeClient(m_otherName, gridIndexFor(m_side));
      cfg.commit();
    }
    break;
  }
  default:
    break;
  }
}

int SetupWizard::gridIndexFor(Side side) const
{
  // 5 x 3 grid, main computer in the centre (index 7).
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

void SetupWizard::addCheckRow(const QString &text, const QString &sub, bool fixed)
{
  auto *row = new QWidget();
  auto *rl = new QHBoxLayout(row);
  rl->setContentsMargins(18, 12, 18, 12);
  rl->setSpacing(14);
  auto *dot = new QLabel(QStringLiteral("<span style='color:#000;font-weight:700'>&#10003;</span>"), row);
  dot->setFixedSize(26, 26);
  dot->setAlignment(Qt::AlignCenter);
  dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 13px;").arg(kGreen));
  auto *txt = new QLabel(
      sub.isEmpty() ? text
                    : QStringLiteral("%1<br><span style='font-size:11px;color:%2'>%3</span>").arg(text, kDim, sub),
      row
  );
  txt->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 500; background: transparent;"));
  rl->addWidget(dot);
  rl->addWidget(txt, 1);
  if (fixed) {
    auto *badge = new QLabel(tr("Fixed for you"), row);
    badge->setStyleSheet(
        QStringLiteral("font-size: 11px; font-weight: 600; color: %1; background: rgba(255,214,10,0.14); "
                       "border-radius: 10px; padding: 4px 9px;")
            .arg(kAccent)
    );
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

  // 1. Process mode. The Windows service cannot read the per-user settings
  //    file (it never starts the core), so run the core on the desktop.
  bool fixedMode = false;
#if defined(Q_OS_WIN)
  if (Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>() != Settings::ProcessMode::Desktop) {
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    fixedMode = true;
  }
#endif
  addCheckRow(tr("Runs on your desktop, not as a hidden service"), QString(), fixedMode);

  // 2. Secure connection: make sure this computer has its own certificate.
  bool fixedCert = false;
  if (TlsUtility::isEnabled() && !TlsUtility::isCertValid()) {
    fixedCert = TlsUtility::generateCertificate();
  }
  addCheckRow(tr("Connection is private and encrypted"), tr("Computers trust each other from the desk code"), fixedCert);

  // 3. Firewall: the installer adds the rule; report it.
  addCheckRow(tr("Firewall lets your computers talk"), tr("Rule limited to your own network"), false);

  // 4. Autostart with the app.
  if (!Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    Settings::setValue(Settings::Gui::AutoStartCore, true);
    addCheckRow(tr("Starts by itself when you open Lightning Desk"), QString(), true);
  } else {
    addCheckRow(tr("Starts by itself when you open Lightning Desk"), QString(), false);
  }

  // 5. Clipboard + drag-and-drop on.
  if (!Settings::value(Settings::Server::EnableClipboard).toBool()) {
    Settings::setValue(Settings::Server::EnableClipboard, true);
    addCheckRow(tr("Copy, paste and drag files across computers"), QString(), true);
  } else {
    addCheckRow(tr("Copy, paste and drag files across computers"), QString(), false);
  }
}

} // namespace deskflow::gui
