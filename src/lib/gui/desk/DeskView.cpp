/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/DeskView.h"

#include "common/Settings.h"
#include "desk/BrandIcons.h"
#include "gui/config/ServerConfig.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;
using deskflow::core::ConnectionState;

namespace deskflow::gui {

namespace {

constexpr int kCardW = 188;
constexpr int kCardH = 128;
constexpr int kArrow = 28;

QString pillStyle(const QString &fg, const QString &bg)
{
  return QStringLiteral(
             "QLabel{color:%1;background:%2;border-radius:9px;padding:2px 8px;font-size:11px;font-weight:600;}"
  )
      .arg(fg, bg);
}

QString switchStyle()
{
  return QStringLiteral(
             "QPushButton{min-width:44px;max-width:44px;min-height:24px;max-height:24px;border-radius:12px;"
             "background:%1;border:none;color:transparent;}"
             "QPushButton:checked{background:%2;}"
             "QPushButton:disabled{background:%3;}"
  )
      .arg(kLine, kGreen, kCard2);
}

QLabel *label(const QString &text, const QString &color, int px, int weight = 400, QWidget *parent = nullptr)
{
  auto *l = new QLabel(text, parent);
  l->setStyleSheet(
      QStringLiteral("color:%1;font-size:%2px;font-weight:%3;background:transparent;").arg(color).arg(px).arg(weight)
  );
  return l;
}

} // namespace

struct DeskView::Card
{
  QFrame *frame = nullptr;
  QLabel *icon = nullptr;
  QLabel *pill = nullptr;
  QLabel *name = nullptr;
  QLabel *sub = nullptr;
};

DeskView::DeskView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("deskView"));
  setStyleSheet(QStringLiteral("#deskView{background:%1;}").arg(kBg));

  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);
  outer->addWidget(buildRail());

  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:%1;").arg(kBg));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);
  col->addWidget(buildHeader());
  col->addWidget(buildGrid(), 1, Qt::AlignHCenter | Qt::AlignTop);
  col->addStretch(1);
  col->addWidget(buildConveniences());

  auto *scroll = new QScrollArea(this);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setWidget(content);
  scroll->setStyleSheet(QStringLiteral(
                            "QScrollArea{background:%1;border:none;}"
                            "QScrollBar:vertical{background:%1;width:8px;margin:0;}"
                            "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:24px;}"
                            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
                            "QScrollBar:horizontal{background:%1;height:8px;margin:0;}"
                            "QScrollBar::handle:horizontal{background:%2;border-radius:4px;min-width:24px;}"
                            "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
  )
                            .arg(kBg, kLine));
  outer->addWidget(scroll, 1);

  setConnectionState(ConnectionState::Disconnected);
  refreshConveniences();
}

QWidget *DeskView::buildRail()
{
  auto *rail = new QFrame(this);
  rail->setFixedWidth(176);
  rail->setObjectName(QStringLiteral("deskRail"));
  rail->setStyleSheet(
      QStringLiteral("QFrame#deskRail{background:%1;border-right:1px solid %2;}QLabel{border:none;}").arg(kBg, kLine)
  );
  auto *v = new QVBoxLayout(rail);
  v->setContentsMargins(14, 18, 14, 18);
  v->setSpacing(6);

  auto *brandRow = new QHBoxLayout();
  auto *bolt = new QLabel(rail);
  bolt->setPixmap(tile(Icon::Bolt, 28, devicePixelRatioF(), kAccent, kBg));
  bolt->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
  brandRow->addWidget(bolt);
  brandRow->addWidget(label(tr("Lightning Desk"), QStringLiteral("#FFFFFF"), 13, 700, rail));
  brandRow->addStretch();
  v->addLayout(brandRow);
  v->addSpacing(16);

  const auto navStyle = QStringLiteral(
                            "QPushButton{color:%1;background:transparent;border:none;border-radius:8px;"
                            "padding:8px 10px;text-align:left;font-size:13px;}"
                            "QPushButton:hover{background:%2;}"
                            "QPushButton:checked{background:%2;color:#FFFFFF;}"
  )
                            .arg(kDim, kCard);

  auto makeNav = [&](Icon ic, const QString &text, bool checked) {
    auto *b = new QPushButton(text, rail);
    b->setIcon(QIcon(icon(ic, 18, devicePixelRatioF(), checked ? kAccent : kDim)));
    b->setCheckable(true);
    b->setChecked(checked);
    b->setAutoExclusive(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(navStyle);
    v->addWidget(b);
    return b;
  };
  makeNav(Icon::Monitor, tr("Desk"), true);
  auto *shortcuts = makeNav(Icon::Keyboard, tr("Shortcuts"), false);
  auto *settings = makeNav(Icon::Gear, tr("Settings"), false);
  connect(shortcuts, &QPushButton::clicked, this, &DeskView::shortcutsRequested);
  connect(settings, &QPushButton::clicked, this, &DeskView::settingsRequested);

  v->addStretch(1);

  auto *help = new QPushButton(tr("Help"), rail);
  help->setIcon(QIcon(icon(Icon::Help, 18, devicePixelRatioF(), kDim)));
  help->setCursor(Qt::PointingHandCursor);
  help->setStyleSheet(navStyle);
  connect(help, &QPushButton::clicked, this, &DeskView::helpRequested);
  v->addWidget(help);

  auto *statusRow = new QHBoxLayout();
  statusRow->setContentsMargins(10, 6, 0, 0);
  m_railStatusDot = label(QStringLiteral("●"), kGreen, 11, 400, rail);
  m_railStatusText = label(tr("On"), kDim, 12, 400, rail);
  statusRow->addWidget(m_railStatusDot);
  statusRow->addWidget(m_railStatusText);
  statusRow->addStretch();
  v->addLayout(statusRow);
  return rail;
}

QWidget *DeskView::buildHeader()
{
  auto *w = new QWidget(this);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *h = new QHBoxLayout(w);
  h->setContentsMargins(0, 0, 0, 0);

  auto *titles = new QVBoxLayout();
  titles->setSpacing(4);
  titles->addWidget(label(tr("Your desk"), QStringLiteral("#FFFFFF"), 22, 700, w));
  m_subtitle = label(
      tr("Move the mouse off an edge to switch computers. Drag a card to change where it sits."), kDim, 13, 400, w
  );
  m_subtitle->setWordWrap(true);
  titles->addWidget(m_subtitle);
  h->addLayout(titles, 1);

  m_sharingPill = new QPushButton(w);
  m_sharingPill->setCheckable(true);
  m_sharingPill->setCursor(Qt::PointingHandCursor);
  m_sharingPill->setStyleSheet(
      QStringLiteral(
          "QPushButton{color:%1;background:%2;border:1px solid %3;border-radius:14px;padding:5px 14px;"
          "font-size:12px;font-weight:600;}"
          "QPushButton:checked{color:%4;background:rgba(52,199,89,0.14);border-color:rgba(52,199,89,0.5);}"
      )
          .arg(kDim, kCard, kLine, kGreen)
  );
  connect(m_sharingPill, &QPushButton::toggled, this, [this](bool on) {
    m_sharingPill->setText(on ? tr("Sharing on") : tr("Sharing off"));
    Q_EMIT sharingToggled(on);
  });
  h->addWidget(m_sharingPill, 0, Qt::AlignTop);

  m_addButton = new QPushButton(tr("+ Add a computer"), w);
  m_addButton->setCursor(Qt::PointingHandCursor);
  m_addButton->setStyleSheet(QStringLiteral(
                                 "QPushButton{color:%1;background:%2;border:none;border-radius:14px;padding:6px 16px;"
                                 "font-size:12px;font-weight:700;}QPushButton:hover{background:#FFE14D;}"
  )
                                 .arg(kBg, kAccent));
  connect(m_addButton, &QPushButton::clicked, this, [this] { Q_EMIT addComputerRequested(Side::Right); });
  h->addSpacing(10);
  h->addWidget(m_addButton, 0, Qt::AlignTop);
  return w;
}

QWidget *DeskView::buildGrid()
{
  m_gridHost = new QWidget(this);
  m_gridHost->setStyleSheet(QStringLiteral("background:transparent;"));
  m_grid = new QGridLayout(m_gridHost);
  m_grid->setContentsMargins(0, 8, 0, 8);
  m_grid->setHorizontalSpacing(6);
  m_grid->setVerticalSpacing(6);
  return m_gridHost;
}

QWidget *DeskView::buildConveniences()
{
  auto *w = new QWidget(this);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *h = new QHBoxLayout(w);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(12);

  auto makeBox = [&](Icon ic, const QString &title, const QString &sub) {
    auto *f = new QFrame(w);
    f->setObjectName(QStringLiteral("deskBox"));
    f->setStyleSheet(
        QStringLiteral("QFrame#deskBox{background:%1;border:1px solid %2;border-radius:12px;}QLabel{border:none;}")
            .arg(kCard, kLine)
    );
    auto *row = new QHBoxLayout(f);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(10);
    auto *i = new QLabel(f);
    i->setPixmap(tile(ic, 32, devicePixelRatioF(), kCard2, kAccent));
    i->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    row->addWidget(i);
    auto *txt = new QVBoxLayout();
    txt->setSpacing(2);
    txt->addWidget(label(title, QStringLiteral("#FFFFFF"), 13, 600, f));
    auto *s = label(sub, kDim, 11, 400, f);
    txt->addWidget(s);
    row->addLayout(txt, 1);
    h->addWidget(f, 1);
    return std::make_pair(row, s);
  };

  auto [clipRow, clipSub] = makeBox(Icon::Clipboard, tr("Copy & paste"), tr("Between every computer"));
  m_clipboardSwitch = new QPushButton(w);
  m_clipboardSwitch->setCheckable(true);
  m_clipboardSwitch->setCursor(Qt::PointingHandCursor);
  m_clipboardSwitch->setStyleSheet(switchStyle());
  connect(m_clipboardSwitch, &QPushButton::toggled, this, [this](bool on) {
    Settings::setValue(Settings::Server::EnableClipboard, on);
    Q_EMIT clipboardToggled(on);
  });
  clipRow->addWidget(m_clipboardSwitch);

  auto [fileRow, fileSub] = makeBox(Icon::File, tr("Drag files"), tr("Coming soon"));
  m_fileSwitch = new QPushButton(w);
  m_fileSwitch->setCheckable(true);
  m_fileSwitch->setEnabled(false);
  m_fileSwitch->setToolTip(tr("Dragging files between computers isn't ready yet."));
  m_fileSwitch->setStyleSheet(switchStyle());
  fileRow->addWidget(m_fileSwitch);

  auto [encRow, encSub] = makeBox(Icon::Lock, tr("Encryption"), tr("Always on"));
  m_encryptionValue = encSub;
  Q_UNUSED(encRow)
  Q_UNUSED(clipSub)
  Q_UNUSED(fileSub)
  return w;
}

DeskView::Card *DeskView::makeCard(QWidget *parent)
{
  auto *c = new Card;
  c->frame = new QFrame(parent);
  c->frame->setObjectName(QStringLiteral("deskCard"));
  c->frame->setFixedSize(kCardW, kCardH);
  c->frame->setCursor(Qt::PointingHandCursor);
  c->frame->installEventFilter(this);
  auto *v = new QVBoxLayout(c->frame);
  v->setContentsMargins(14, 12, 14, 12);
  v->setSpacing(4);
  auto *top = new QHBoxLayout();
  c->icon = new QLabel(c->frame);
  c->icon->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
  top->addWidget(c->icon);
  top->addStretch();
  c->pill = new QLabel(c->frame);
  top->addWidget(c->pill, 0, Qt::AlignTop);
  v->addLayout(top);
  v->addStretch();
  c->name = label(QString(), QStringLiteral("#FFFFFF"), 14, 600, c->frame);
  c->sub = label(QString(), kDim, 11, 400, c->frame);
  c->sub->setWordWrap(true);
  v->addWidget(c->name);
  v->addWidget(c->sub);
  return c;
}

QWidget *DeskView::makeEmptyCell(Side side, QWidget *parent)
{
  auto *b = new QPushButton(parent);
  b->setFixedSize(kCardW, kCardH);
  b->setCursor(Qt::PointingHandCursor);
  b->setIcon(QIcon(icon(Icon::Plus, 20, devicePixelRatioF(), kDim)));
  b->setText(tr("Add here"));
  b->setStyleSheet(
      QStringLiteral(
          "QPushButton{color:%1;background:transparent;border:1px dashed %2;border-radius:14px;font-size:12px;}"
          "QPushButton:hover{border-color:%3;color:%3;}"
      )
          .arg(kDim, kLine, kAccent)
  );
  connect(b, &QPushButton::clicked, this, [this, side] { Q_EMIT addComputerRequested(side); });
  return b;
}

void DeskView::fillCard(Card *card, const QString &name, const QString &subtitle, bool isMain)
{
  const bool laptop = name.contains(QStringLiteral("book"), Qt::CaseInsensitive) ||
                      name.contains(QStringLiteral("laptop"), Qt::CaseInsensitive);
  card->icon->setPixmap(tile(
      laptop ? Icon::Laptop : Icon::Monitor, 36, devicePixelRatioF(), isMain ? kAccent : kCard2, isMain ? kBg : kAccent
  ));
  card->name->setText(name);
  card->sub->setText(subtitle);
  m_cardNames.insert(card->frame, name);

  if (isMain) {
    card->frame->setStyleSheet(
        QStringLiteral("QFrame#deskCard{background:%1;border:2px solid %2;border-radius:14px;}QLabel{border:none;}")
            .arg(kCard, kAccent)
    );
    card->pill->setText(tr("Main"));
    card->pill->setStyleSheet(pillStyle(kBg, kAccent));
    return;
  }

  card->frame->setStyleSheet(QStringLiteral(
                                 "QFrame#deskCard{background:%1;border:1px solid %2;border-radius:14px;}"
                                 "QFrame#deskCard:hover{border-color:%3;}QLabel{border:none;}"
  )
                                 .arg(kCard, kLine, kMuted));
  const auto status = statusFor(name);
  if (status == tr("Linked"))
    card->pill->setStyleSheet(pillStyle(kGreen, QStringLiteral("rgba(52,199,89,0.14)")));
  else if (status == tr("Off"))
    card->pill->setStyleSheet(pillStyle(kDim, kCard2));
  else
    card->pill->setStyleSheet(pillStyle(kAccent, QStringLiteral("rgba(255,214,10,0.14)")));
  card->pill->setText(status);
}

QString DeskView::statusFor(const QString &name) const
{
  if (m_connected.contains(name))
    return tr("Linked");
  if (!m_sharing || m_state == ConnectionState::Disconnected)
    return tr("Off");
  return tr("Reconnecting…");
}

void DeskView::relayout()
{
  // Clear the grid.
  while (auto *item = m_grid->takeAt(0)) {
    if (auto *w = item->widget()) {
      // Hide and detach now: deleteLater() waits for the outer event loop, and a nested
      // dialog loop would otherwise leave ghosts of the old cards on screen.
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }
  m_cardNames.clear();

  // 5x5 layout: cards on even rows/cols, arrows on the odd ones between them.
  auto arrow = [&](Icon ic, int r, int c) {
    auto *l = new QLabel(m_gridHost);
    l->setPixmap(icon(ic, kArrow, devicePixelRatioF(), kLine));
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral("background:transparent;"));
    m_grid->addWidget(l, r, c, Qt::AlignCenter);
  };

  const auto sideSub = [&](Side side) {
    switch (side) {
    case Side::Above:
      return tr("Above");
    case Side::Left:
      return tr("Left");
    case Side::Right:
      return tr("Right");
    case Side::Below:
      return tr("Below");
    }
    return QString();
  };

  auto place = [&](Side side, int r, int c) {
    const auto name = m_bySide.value(side);
    if (name.isEmpty()) {
      m_grid->addWidget(makeEmptyCell(side, m_gridHost), r, c, Qt::AlignCenter);
      return;
    }
    auto *card = makeCard(m_gridHost);
    fillCard(card, name, sideSub(side), false);
    m_grid->addWidget(card->frame, r, c, Qt::AlignCenter);
    delete card; // widgets are owned by the grid host; the struct is only a handle
  };

  if (m_serverMode) {
    place(Side::Above, 0, 2);
    arrow(Icon::ArrowUp, 1, 2);
    place(Side::Left, 2, 0);
    arrow(Icon::ArrowLeft, 2, 1);
    auto *main = makeCard(m_gridHost);
    fillCard(main, m_mainName, tr("Mouse & keyboard are here"), true);
    m_grid->addWidget(main->frame, 2, 2, Qt::AlignCenter);
    delete main;
    arrow(Icon::ArrowRight, 2, 3);
    place(Side::Right, 2, 4);
    arrow(Icon::ArrowDown, 3, 2);
    place(Side::Below, 4, 2);
  } else {
    auto *main = makeCard(m_gridHost);
    fillCard(main, m_mainName, tr("Mouse & keyboard are there"), true);
    m_grid->addWidget(main->frame, 2, 0, Qt::AlignCenter);
    delete main;
    arrow(Icon::ArrowRight, 2, 1);
    auto *me = makeCard(m_gridHost);
    const auto status =
        m_state == ConnectionState::Connected
            ? tr("This computer · linked")
            : (m_state == ConnectionState::Connecting ? tr("This computer · connecting…") : tr("This computer · off"));
    fillCard(me, m_thisName, status, false);
    m_grid->addWidget(me->frame, 2, 2, Qt::AlignCenter);
    delete me;
  }
}

void DeskView::setDesk(const QString &mainName, const ServerConfig &config)
{
  m_serverMode = true;
  m_mainName = mainName;
  m_bySide.clear();

  const auto &list = config.computers();
  int mainIdx = -1;
  for (int i = 0; i < list.size(); ++i) {
    if (!list[i].isNull() && list[i].name() == mainName) {
      mainIdx = i;
      break;
    }
  }
  if (mainIdx < 0 && config.centreIndex() < list.size() && !list[config.centreIndex()].isNull())
    mainIdx = config.centreIndex();

  if (mainIdx >= 0) {
    m_bySide.insert(Side::Above, config.neighbourName(mainIdx, 0, -1));
    m_bySide.insert(Side::Left, config.neighbourName(mainIdx, -1, 0));
    m_bySide.insert(Side::Right, config.neighbourName(mainIdx, 1, 0));
    m_bySide.insert(Side::Below, config.neighbourName(mainIdx, 0, 1));
  }

  // Anything configured but not adjacent still deserves a slot: fill empty sides in order.
  QStringList placed = m_bySide.values();
  for (const auto &computer : list) {
    if (computer.isNull() || computer.name() == mainName || placed.contains(computer.name()))
      continue;
    for (auto side : {Side::Right, Side::Left, Side::Below, Side::Above}) {
      if (m_bySide.value(side).isEmpty()) {
        m_bySide.insert(side, computer.name());
        placed << computer.name();
        break;
      }
    }
  }

  m_subtitle->setText(tr("Move the mouse off an edge to switch computers. Drag a card to change where it sits."));
  m_addButton->setVisible(true);
  m_sharingPill->setVisible(true);
  relayout();
}

void DeskView::setLinkedTo(const QString &thisName, const QString &mainName)
{
  m_serverMode = false;
  m_thisName = thisName;
  m_mainName = mainName;
  m_bySide.clear();
  m_subtitle->setText(tr("This computer is an extra screen of %1. Its mouse and keyboard reach here.").arg(mainName));
  m_addButton->setVisible(false);
  m_sharingPill->setVisible(true);
  relayout();
}

void DeskView::setConnectedClients(const QStringList &names)
{
  m_connected = names;
  relayout();
}

void DeskView::setConnectionState(ConnectionState state)
{
  m_state = state;
  QString text;
  QString color;
  switch (state) {
  case ConnectionState::Connected:
    text = m_serverMode ? tr("On") : tr("Linked");
    color = kGreen;
    break;
  case ConnectionState::Listening:
    text = tr("On · waiting");
    color = kGreen;
    break;
  case ConnectionState::Connecting:
    text = tr("Starting…");
    color = kAccent;
    break;
  case ConnectionState::Disconnected:
  default:
    text = tr("Off");
    color = kDim;
    break;
  }
  m_railStatusDot->setStyleSheet(QStringLiteral("color:%1;background:transparent;font-size:11px;").arg(color));
  m_railStatusText->setText(text);
  relayout();
}

void DeskView::setSharing(bool on)
{
  m_sharing = on;
  const QSignalBlocker block(m_sharingPill);
  m_sharingPill->setChecked(on);
  m_sharingPill->setText(on ? tr("Sharing on") : tr("Sharing off"));
  relayout();
}

void DeskView::setTrustedCount(int count)
{
  m_trusted = count;
  refreshConveniences();
}

void DeskView::refreshConveniences()
{
  {
    const QSignalBlocker block(m_clipboardSwitch);
    m_clipboardSwitch->setChecked(Settings::value(Settings::Server::EnableClipboard).toBool());
  }
  const bool tls = Settings::value(Settings::Security::TlsEnabled).toBool();
  if (!tls) {
    m_encryptionValue->setText(tr("Off · turn on in Settings"));
    return;
  }
  if (m_trusted <= 0)
    m_encryptionValue->setText(tr("Always on"));
  else
    m_encryptionValue->setText(tr("Always on · %n trusted", "", m_trusted));
}

bool DeskView::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::MouseButtonRelease) {
    const auto name = m_cardNames.value(watched);
    if (!name.isEmpty() && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
      Q_EMIT computerClicked(name);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

} // namespace deskflow::gui
