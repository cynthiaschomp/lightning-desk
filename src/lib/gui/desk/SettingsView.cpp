/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/SettingsView.h"

#include "common/I18N.h"
#include "common/Settings.h"
#include "desk/BrandIcons.h"
#include "gui/core/NetworkMonitor.h"

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
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

QString switchStyle()
{
  return QStringLiteral(
             "QPushButton{min-width:44px;max-width:44px;min-height:24px;max-height:24px;border-radius:12px;"
             "background:%1;border:none;color:transparent;}"
             "QPushButton:checked{background:%2;}QPushButton:disabled{background:%3;}"
  )
      .arg(kLine, kGreen, kCard2);
}

QString comboStyle()
{
  return QStringLiteral(
             "QComboBox{color:#FFFFFF;background:%1;border:1px solid %2;border-radius:9px;padding:5px 10px;"
             "font-size:13px;min-width:120px;}QComboBox::drop-down{border:none;width:18px;}"
             "QComboBox QAbstractItemView{background:%1;color:#FFFFFF;selection-background-color:%3;"
             "border:1px solid %2;}"
             "QSpinBox{color:#FFFFFF;background:%1;border:1px solid %2;border-radius:9px;padding:5px 10px;"
             "font-size:13px;min-width:90px;}"
  )
      .arg(kCard2, kLine, kCard);
}

QPushButton *smallButton(const QString &t, bool primary, QWidget *parent)
{
  auto *b = new QPushButton(t, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{background:%1;color:%2;border:none;border-radius:10px;padding:7px 14px;"
                       "font-size:12px;font-weight:600;}QPushButton:hover{background:%3;}"
  )
                       .arg(
                           primary ? kAccent : kCard2, primary ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF"),
                           primary ? QStringLiteral("#FFE14D") : kLine
                       ));
  return b;
}

} // namespace

SettingsView::SettingsView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("settingsView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#settingsView{background:%1;}").arg(kBg) + comboStyle());

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);

  col->addWidget(text(tr("Settings"), kText, 22, 700, content));

  // --- Your plan -------------------------------------------------------------------------
  auto *plan = new QFrame(content);
  plan->setObjectName(QStringLiteral("planRow"));
  plan->setCursor(Qt::PointingHandCursor);
  plan->setStyleSheet(QStringLiteral(
                          "QFrame#planRow{background:%1;border:1px solid %2;border-radius:12px;}"
                          "QFrame#planRow:hover{border-color:%3;}QLabel{border:none;}"
  )
                          .arg(kCard, kLine, kMuted));
  auto *ph = new QHBoxLayout(plan);
  ph->setContentsMargins(16, 12, 16, 12);
  ph->setSpacing(14);
  auto *bolt = new QLabel(plan);
  bolt->setPixmap(tile(Icon::Bolt, 36, devicePixelRatioF(), kAccent, kBg));
  ph->addWidget(bolt);
  auto *pt = new QVBoxLayout();
  pt->setSpacing(2);
  m_planTitle = text(tr("Lightning Desk Free"), kText, 14, 600, plan);
  m_planDetail = text(QString(), kDim, 12, 400, plan);
  pt->addWidget(m_planTitle);
  pt->addWidget(m_planDetail);
  ph->addLayout(pt, 1);
  m_planPill = new QLabel(plan);
  m_planPill->setStyleSheet(QStringLiteral(
                                "color:%1;background:rgba(255,214,10,0.14);border-radius:9px;padding:3px 9px;"
                                "font-size:11px;font-weight:700;"
  )
                                .arg(kAccent));
  ph->addWidget(m_planPill);
  ph->addWidget(text(QStringLiteral("›"), kDim, 18, 400, plan));
  m_planRow = plan;
  plan->installEventFilter(this);
  auto *planWrap = new QVBoxLayout();
  planWrap->setSpacing(8);
  planWrap->addWidget(text(tr("YOUR PLAN"), kDim, 11, 700, content));
  planWrap->addWidget(plan);
  col->addLayout(planWrap);

  // --- Two columns -----------------------------------------------------------------------
  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(24);
  grid->setVerticalSpacing(18);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);

  // General
  m_language = new QComboBox(content);
  I18N::reDetectLanguages();
  m_language->addItems(I18N::detectedLanguages());
  connect(m_language, &QComboBox::currentTextChanged, this, [this](const QString &native) {
    if (m_loading)
      return;
    const auto shortName = I18N::nativeTo639Name(native);
    I18N::setLanguage(shortName);
    Settings::setValue(Settings::Core::Language, shortName);
  });
  auto *general = section(
      tr("General"),
      {row(tr("Keep sharing when Lightning Desk opens"), tr("Your desk is live as soon as the app starts"),
           makeSwitch(Settings::Gui::AutoStartCore, false, false), false),
       row(tr("Closing the window keeps it running"), tr("It stays in the menu bar / tray; sharing carries on"),
           makeSwitch(Settings::Gui::CloseToTray, false, false), false),
       row(tr("Start hidden in the tray"), tr("Open the window from the tray icon when you need it"),
           makeSwitch(Settings::Gui::Autohide, false, false), false),
       row(tr("Tell me when an update is ready"), tr("Nothing installs without you"),
           makeSwitch(Settings::Gui::AutoUpdateCheck, false, false), false),
       row(tr("Language"), QString(), m_language, true)}
  );
  grid->addWidget(general, 0, 0, Qt::AlignTop);

  // Sharing
  auto *fileSwitch = new QPushButton(content);
  fileSwitch->setCheckable(true);
  fileSwitch->setEnabled(false);
  fileSwitch->setStyleSheet(switchStyle());
  fileSwitch->setToolTip(tr("Dragging files between computers isn't ready yet."));
  auto *sharing = section(
      tr("Sharing"), {row(tr("Copy and paste across computers"), tr("Text, images and files on the clipboard"),
                          makeSwitch(Settings::Server::EnableClipboard, false, true), false),
                      row(tr("Drag files across computers"), tr("Coming soon"), fileSwitch, true)}
  );
  grid->addWidget(sharing, 1, 0, Qt::AlignTop);

  // Switching
  m_delay = new QComboBox(content);
  m_delay->addItem(tr("Instantly"), 0);
  m_delay->addItem(tr("After a short pause (¼ s)"), 250);
  m_delay->addItem(tr("After a pause (½ s)"), 500);
  m_delay->addItem(tr("After a long pause (1 s)"), 1000);
  connect(m_delay, &QComboBox::currentIndexChanged, this, [this](int) {
    if (m_loading)
      return;
    const int ms = m_delay->currentData().toInt();
    Settings::setValue(Settings::Server::EnableSwitchDelay, ms > 0);
    if (ms > 0)
      Settings::setValue(Settings::Server::SwitchDelay, ms);
    Q_EMIT serverSettingChanged();
  });
  auto *switching = section(
      tr("Switching"),
      {row(tr("Switch when the mouse touches an edge"), tr("A short pause avoids accidental switches"), m_delay, false),
       row(tr("Double-tap the edge to switch"), tr("Stops you drifting onto the wrong screen"),
           makeSwitch(Settings::Server::EnableSwitchDoubleTap, false, true), false),
       row(tr("Let Scroll Lock keep the mouse on one computer"), tr("Press it again to release"),
           makeSwitch(Settings::Server::DisableLockToComputer, true, true), false),
       row(tr("Prevent this computer from sleeping while linked"), QString(),
           makeSwitch(Settings::Core::PreventSleep, false, false), true)}
  );
  grid->addWidget(switching, 0, 1, Qt::AlignTop);

  // Privacy & security
  auto *encOn = new QHBoxLayout();
  auto *encLabel = text(tr("On"), kGreen, 13, 700, content);
  auto *lock = new QLabel(content);
  lock->setPixmap(icon(Icon::Lock, 14, devicePixelRatioF(), kDim));
  lock->setStyleSheet(QStringLiteral("background:transparent;"));
  encOn->addWidget(encLabel);
  encOn->addWidget(lock);
  auto *encWrap = new QWidget(content);
  encWrap->setStyleSheet(QStringLiteral("background:transparent;"));
  encWrap->setLayout(encOn);
  m_trustedValue = text(QStringLiteral("0"), kMuted, 14, 400, content);
  auto *newKey = smallButton(tr("Generate"), false, content);
  connect(newKey, &QPushButton::clicked, this, &SettingsView::newKeyRequested);
  auto *trustedRow = row(tr("Trusted computers"), tr("Allowed to connect to this desk"), m_trustedValue, false);
  m_trustedNames = trustedRow->findChild<QLabel *>(QStringLiteral("rowSub"));
  auto *security = section(
      tr("Privacy & security"),
      {row(tr("Connections are encrypted"), tr("Always on. Only computers you've allowed can connect."), encWrap,
           false),
       trustedRow, row(tr("New security key"), tr("Other computers will ask you to allow them again"), newKey, true)}
  );
  grid->addWidget(security, 1, 1, Qt::AlignTop);

  // Advanced (expands inline)
  m_advancedToggle = new QPushButton(content);
  m_advancedToggle->setCursor(Qt::PointingHandCursor);
  m_advancedToggle->setStyleSheet(QStringLiteral(
                                      "QPushButton{text-align:left;color:#FFFFFF;background:%1;border:1px solid %2;"
                                      "border-radius:12px;padding:12px 16px;font-size:14px;font-weight:600;}"
                                      "QPushButton:hover{border-color:%3;}"
  )
                                      .arg(kCard, kLine, kMuted));
  m_advancedToggle->setText(tr("Advanced  ›"));
  m_port = new QSpinBox(content);
  m_port->setRange(1024, 65535);
  connect(m_port, &QSpinBox::editingFinished, this, [this] {
    if (m_loading)
      return;
    if (Settings::value(Settings::Core::Port).toInt() != m_port->value()) {
      Settings::setValue(Settings::Core::Port, m_port->value());
      Q_EMIT coreSettingChanged();
    }
  });
  m_interface = new QComboBox(content);
  connect(m_interface, &QComboBox::currentIndexChanged, this, [this](int) {
    if (m_loading)
      return;
    Settings::setValue(Settings::Core::Interface, m_interface->currentData().toString());
    Q_EMIT coreSettingChanged();
  });
  auto *logs = smallButton(tr("Open log folder"), false, content);
  connect(logs, &QPushButton::clicked, this, &SettingsView::openLogsRequested);
  auto *reset = smallButton(tr("Reset…"), false, content);
  m_resetConfirm = new QWidget(content);
  m_resetConfirm->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *rc = new QHBoxLayout(m_resetConfirm);
  rc->setContentsMargins(0, 0, 0, 0);
  rc->setSpacing(8);
  rc->addWidget(text(tr("Everything back to defaults, including linked computers?"), kDim, 12, 400, m_resetConfirm), 1);
  auto *keep = smallButton(tr("Keep"), false, m_resetConfirm);
  auto *doReset = smallButton(tr("Reset everything"), true, m_resetConfirm);
  rc->addWidget(keep);
  rc->addWidget(doReset);
  m_resetConfirm->hide();
  connect(reset, &QPushButton::clicked, this, [this] { m_resetConfirm->setVisible(!m_resetConfirm->isVisible()); });
  connect(keep, &QPushButton::clicked, this, [this] { m_resetConfirm->hide(); });
  connect(doReset, &QPushButton::clicked, this, [this] {
    m_resetConfirm->hide();
    Q_EMIT resetRequested();
  });
  m_advanced = section(
      QString(),
      {row(tr("Port"), tr("Other computers connect to this number. Change it only if something else is using it."),
           m_port, false),
       row(tr("Network address"), tr("Which of this computer's addresses to listen on"), m_interface, false),
       row(tr("Logs"), tr("For a bug report"), logs, false), row(tr("Reset Lightning Desk"), QString(), reset, true)}
  );
  auto *advWrap = new QVBoxLayout();
  advWrap->setSpacing(8);
  advWrap->addWidget(m_advancedToggle);
  advWrap->addWidget(m_advanced);
  advWrap->addWidget(m_resetConfirm);
  m_advanced->hide();
  connect(m_advancedToggle, &QPushButton::clicked, this, [this] {
    const bool open = !m_advanced->isVisible();
    m_advanced->setVisible(open);
    if (!open)
      m_resetConfirm->hide();
    m_advancedToggle->setText(open ? tr("Advanced  ⌄") : tr("Advanced  ›"));
  });
  grid->addLayout(advWrap, 2, 0, 1, 2);

  col->addLayout(grid);
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

  reload();
}

QWidget *SettingsView::section(const QString &title, const QList<QWidget *> &rows)
{
  auto *w = new QWidget(this);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  if (!title.isEmpty()) {
    auto *l = text(title.toUpper(), kDim, 11, 700, w);
    l->setContentsMargins(2, 0, 0, 8);
    v->addWidget(l);
  }
  for (int i = 0; i < rows.size(); ++i) {
    auto *r = rows[i];
    const bool first = i == 0;
    const bool last = i == rows.size() - 1;
    const QString radius = (first && last) ? QStringLiteral("border-radius:12px;")
                           : first         ? QStringLiteral("border-top-left-radius:12px;border-top-right-radius:12px;")
                           : last ? QStringLiteral("border-bottom-left-radius:12px;border-bottom-right-radius:12px;")
                                  : QString();
    r->setStyleSheet(QStringLiteral("QFrame#setRow{background:%1;border:1px solid %2;%3%4}QLabel{border:none;}")
                         .arg(kCard, kLine, radius, last ? QString() : QStringLiteral("border-bottom:none;")));
    v->addWidget(r);
  }
  return w;
}

QWidget *SettingsView::row(const QString &title, const QString &sub, QWidget *control, bool)
{
  auto *r = new QFrame(this);
  r->setObjectName(QStringLiteral("setRow"));
  auto *h = new QHBoxLayout(r);
  h->setContentsMargins(16, 11, 14, 11);
  h->setSpacing(14);
  auto *txt = new QVBoxLayout();
  txt->setSpacing(2);
  txt->addWidget(text(title, kText, 13, 600, r));
  auto *s = text(sub, kDim, 11, 400, r);
  s->setObjectName(QStringLiteral("rowSub"));
  s->setVisible(!sub.isEmpty());
  txt->addWidget(s);
  h->addLayout(txt, 1);
  if (control) {
    control->setParent(r);
    h->addWidget(control, 0, Qt::AlignVCenter);
  }
  return r;
}

QPushButton *SettingsView::makeSwitch(const QString &key, bool invert, bool server)
{
  auto *b = new QPushButton(this);
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(switchStyle());
  m_switches.append({b, key});
  m_switchInverted.append(invert);
  connect(b, &QPushButton::toggled, this, [this, key, invert, server](bool on) {
    if (m_loading)
      return;
    Settings::setValue(key, invert ? !on : on);
    if (server)
      Q_EMIT serverSettingChanged();
    else if (key == Settings::Core::PreventSleep)
      Q_EMIT coreSettingChanged();
  });
  return b;
}

void SettingsView::reload()
{
  m_loading = true;
  for (int i = 0; i < m_switches.size(); ++i) {
    const auto &[button, key] = m_switches[i];
    const bool v = Settings::value(key).toBool();
    button->setChecked(m_switchInverted[i] ? !v : v);
  }
  const bool delayOn = Settings::value(Settings::Server::EnableSwitchDelay).toBool();
  const int ms = delayOn ? Settings::value(Settings::Server::SwitchDelay).toInt() : 0;
  int best = 0;
  for (int i = 0; i < m_delay->count(); ++i)
    if (qAbs(m_delay->itemData(i).toInt() - ms) < qAbs(m_delay->itemData(best).toInt() - ms))
      best = i;
  m_delay->setCurrentIndex(best);
  m_language->setCurrentText(I18N::toNativeName(I18N::currentLanguage()));
  m_port->setValue(Settings::value(Settings::Core::Port).toInt());
  m_interface->clear();
  m_interface->addItem(tr("Any address (recommended)"), QString());
  for (const auto &addr : NetworkMonitor::validAddresses())
    m_interface->addItem(addr, addr);
  const auto current = Settings::value(Settings::Core::Interface).toString();
  const int idx = m_interface->findData(current);
  m_interface->setCurrentIndex(idx < 0 ? 0 : idx);
  m_loading = false;
}

bool SettingsView::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_planRow && event->type() == QEvent::MouseButtonRelease &&
      static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
    Q_EMIT planClicked();
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

void SettingsView::setPlan(const QString &title, const QString &detail, const QString &pillText)
{
  m_planTitle->setText(title);
  m_planDetail->setText(detail);
  m_planPill->setText(pillText);
  m_planPill->setVisible(!pillText.isEmpty());
}

void SettingsView::setTrusted(int count, const QStringList &names)
{
  Q_UNUSED(names)
  m_trustedValue->setText(QString::number(count));
}

} // namespace deskflow::gui
