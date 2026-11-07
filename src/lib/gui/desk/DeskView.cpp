/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/DeskView.h"

#include "common/Settings.h"
#include "desk/AboutView.h"
#include "desk/BrandIcons.h"
#include "desk/ChecksView.h"
#include "desk/ComputerView.h"
#include "desk/NoticeCard.h"
#include "desk/PlanView.h"
#include "desk/SettingsView.h"
#include "desk/ShortcutsView.h"
#include "gui/config/ServerConfig.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>

#include <QVBoxLayout>
#include <algorithm>

using namespace deskflow::gui::brand;
using deskflow::core::ConnectionState;

namespace deskflow::gui {

namespace {

constexpr int kCardWMax = 160;
constexpr int kCardWMin = 132;
constexpr int kCardH = 116;
constexpr int kEmptyH = 64;
constexpr int kArrow = 22;

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
  QToolButton *menu = nullptr;
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
  m_noticeHost = new QVBoxLayout();
  m_noticeHost->setContentsMargins(0, 0, 0, 0);
  col->addLayout(m_noticeHost);
  col->addWidget(buildGrid(), 1, Qt::AlignHCenter | Qt::AlignTop);
  col->addStretch(1);
  col->addWidget(buildConveniences());

  m_deskPage = new QScrollArea(this);
  auto *scroll = static_cast<QScrollArea *>(m_deskPage);
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
  m_shortcuts = new ShortcutsView(this);
  m_settings = new SettingsView(this);
  m_about = new AboutView(this);
  m_plan = new PlanView(this);
  m_checks = new ChecksView(this);
  m_computer = new ComputerView(this);
  m_pages = new QStackedWidget(this);
  m_pages->addWidget(scroll);
  m_pages->addWidget(m_shortcuts);
  m_pages->addWidget(m_settings);
  m_pages->addWidget(m_about);
  m_pages->addWidget(m_plan);
  m_pages->addWidget(m_checks);
  m_pages->addWidget(m_computer);
  connect(m_plan, &PlanView::backRequested, this, &DeskView::showSettingsPage);
  connect(m_checks, &ChecksView::backRequested, this, &DeskView::showAboutPage);
  connect(m_computer, &ComputerView::backRequested, this, &DeskView::showDeskPage);
  connect(m_computer, &ComputerView::moveRequested, this, &DeskView::moveRequested);
  connect(m_computer, &ComputerView::forgetRequested, this, [this](const QString &name) {
    auto *card = notice();
    card->setIcon(Icon::Laptop, NoticeCard::Tone::Danger)
        ->setTitle(tr("Forget %1?").arg(name))
        ->setBody(tr("It comes off your desk and will have to be allowed again to rejoin."))
        ->setButtons({tr("Keep it"), tr("Forget")}, true);
    connect(card, &NoticeCard::answered, this, [this, name](int button, int) {
      dismissNotice();
      if (button == 1)
        Q_EMIT forgetRequested(name);
    });
  });
  outer->addWidget(m_pages, 1);

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
  m_navDesk = makeNav(Icon::Monitor, tr("Desk"), true);
  m_navShortcuts = makeNav(Icon::Keyboard, tr("Shortcuts"), false);
  m_navSettings = makeNav(Icon::Gear, tr("Settings"), false);
  const auto paint = [this](int page) {
    m_navDesk->setIcon(QIcon(icon(Icon::Monitor, 18, devicePixelRatioF(), page == 0 ? kAccent : kDim)));
    m_navShortcuts->setIcon(QIcon(icon(Icon::Keyboard, 18, devicePixelRatioF(), page == 1 ? kAccent : kDim)));
    m_navSettings->setIcon(QIcon(icon(Icon::Gear, 18, devicePixelRatioF(), page == 2 ? kAccent : kDim)));
    if (m_navHelp)
      m_navHelp->setIcon(QIcon(icon(Icon::Help, 18, devicePixelRatioF(), page == 3 ? kAccent : kDim)));
  };
  connect(m_navDesk, &QPushButton::clicked, this, [this, paint] {
    m_pages->setCurrentIndex(0);
    paint(0);
  });
  connect(m_navShortcuts, &QPushButton::clicked, this, [this, paint] {
    m_pages->setCurrentIndex(1);
    paint(1);
    Q_EMIT shortcutsRequested();
  });
  connect(m_navSettings, &QPushButton::clicked, this, [this, paint] {
    m_pages->setCurrentIndex(2);
    paint(2);
    Q_EMIT settingsRequested();
  });

  v->addStretch(1);

  m_navHelp = new QPushButton(tr("Help"), rail);
  m_navHelp->setIcon(QIcon(icon(Icon::Help, 18, devicePixelRatioF(), kDim)));
  m_navHelp->setCheckable(true);
  m_navHelp->setAutoExclusive(true);
  m_navHelp->setCursor(Qt::PointingHandCursor);
  m_navHelp->setStyleSheet(navStyle);
  connect(m_navHelp, &QPushButton::clicked, this, [this, paint] {
    m_pages->setCurrentIndex(3);
    paint(3);
    Q_EMIT helpRequested();
  });
  v->addWidget(m_navHelp);

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
  connect(m_addButton, &QPushButton::clicked, this, [this] { Q_EMIT addComputerRequested(firstFreeNearMain()); });
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
  c->frame->setFixedSize(m_cardW, kCardH);
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
  c->menu = new QToolButton(c->frame);
  c->menu->setText(QStringLiteral("\u22EF"));
  c->menu->setToolTip(tr("Move or forget this computer"));
  c->menu->setCursor(Qt::PointingHandCursor);
  c->menu->setFixedSize(24, 22);
  c->menu->setStyleSheet(QStringLiteral(
                             "QToolButton{color:%1;background:transparent;border:none;border-radius:6px;font-size:16px;"
                             "font-weight:700;padding-bottom:6px;}QToolButton:hover{background:%2;color:#FFFFFF;}"
  )
                             .arg(kDim, kCard2));
  top->addSpacing(4);
  top->addWidget(c->menu, 0, Qt::AlignTop);
  v->addLayout(top);
  v->addStretch();
  c->name = label(QString(), QStringLiteral("#FFFFFF"), 14, 600, c->frame);
  c->sub = label(QString(), kDim, 11, 400, c->frame);
  c->sub->setWordWrap(true);
  v->addWidget(c->name);
  v->addWidget(c->sub);
  return c;
}

QWidget *DeskView::makeEmptyCell(int cell, QWidget *parent)
{
  auto *b = new QPushButton(parent);
  b->setFixedSize(m_cardW, kEmptyH);
  b->setCursor(Qt::PointingHandCursor);
  b->setIcon(QIcon(icon(Icon::Plus, 20, devicePixelRatioF(), kDim)));
  b->setText(tr("Add here"));
  b->setToolTip(placeLabel(cell));
  b->setStyleSheet(
      QStringLiteral(
          "QPushButton{color:%1;background:transparent;border:1px dashed %2;border-radius:12px;font-size:12px;}"
          "QPushButton:hover{border-color:%3;color:%3;}"
      )
          .arg(kDim, kLine, kAccent)
  );
  connect(b, &QPushButton::clicked, this, [this, cell] { Q_EMIT addComputerRequested(cell); });
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
  card->menu->setVisible(!isMain && m_serverMode);
  connect(card->menu, &QToolButton::clicked, this, [this, name] { Q_EMIT computerClicked(name); });

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
  return tr("Waiting");
}

bool DeskView::occupied(int cell) const
{
  return m_cells.contains(cell);
}

bool DeskView::nextToOccupied(int cell) const
{
  const int r = cell / m_columns;
  const int c = cell % m_columns;
  const auto has = [&](int rr, int cc) {
    return rr >= 0 && rr < m_rows && cc >= 0 && cc < m_columns && occupied(rr * m_columns + cc);
  };
  return has(r - 1, c) || has(r + 1, c) || has(r, c - 1) || has(r, c + 1);
}

int DeskView::firstFreeNearMain() const
{
  const int r = m_mainCell / m_columns;
  const int c = m_mainCell % m_columns;
  const int order[][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {0, 2}, {0, -2}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for (const auto &d : order) {
    const int rr = r + d[0];
    const int cc = c + d[1];
    if (rr < 0 || rr >= m_rows || cc < 0 || cc >= m_columns)
      continue;
    const int cell = rr * m_columns + cc;
    if (!occupied(cell) && nextToOccupied(cell))
      return cell;
  }
  for (int cell = 0; cell < m_columns * m_rows; ++cell)
    if (!occupied(cell))
      return cell;
  return -1;
}

QString DeskView::placeLabel(int cell) const
{
  const int dr = cell / m_columns - m_mainCell / m_columns;
  const int dc = cell % m_columns - m_mainCell % m_columns;
  QString v;
  if (dr < 0)
    v = tr("Above");
  else if (dr > 0)
    v = tr("Below");
  QString h;
  if (dc == -1)
    h = tr("Left");
  else if (dc == 1)
    h = tr("Right");
  else if (dc < -1)
    h = tr("Far left");
  else if (dc > 1)
    h = tr("Far right");
  if (!v.isEmpty() && !h.isEmpty())
    return tr("%1, %2 of %3").arg(v, h.toLower(), m_mainName);
  if (!v.isEmpty())
    return tr("%1 %2").arg(v, m_mainName);
  return tr("%1 of %2").arg(h, m_mainName);
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

  // Cards on even rows/cols, arrows on the odd ones between adjacent computers.
  auto arrow = [&](Icon ic, int r, int c) {
    auto *l = new QLabel(m_gridHost);
    l->setPixmap(icon(ic, kArrow, devicePixelRatioF(), kLine));
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral("background:transparent;"));
    m_grid->addWidget(l, r, c, Qt::AlignCenter);
  };
  auto spacer = [&](int r, int c, int w, int h) {
    auto *s = new QWidget(m_gridHost);
    s->setFixedSize(w, h);
    s->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_grid->addWidget(s, r, c, Qt::AlignCenter);
  };

  if (!m_serverMode) {
    auto *main = makeCard(m_gridHost);
    fillCard(main, m_mainName, tr("Mouse & keyboard are there"), true);
    m_grid->addWidget(main->frame, 0, 0, Qt::AlignCenter);
    delete main;
    arrow(Icon::ArrowRight, 0, 1);
    auto *me = makeCard(m_gridHost);
    const auto status =
        m_state == ConnectionState::Connected
            ? tr("This computer · linked")
            : (m_state == ConnectionState::Connecting ? tr("This computer · connecting…") : tr("This computer · off"));
    fillCard(me, m_thisName, status, false);
    m_grid->addWidget(me->frame, 0, 2, Qt::AlignCenter);
    delete me;
    return;
  }

  // Only draw the rows/columns that have something in them (or a free cell beside something),
  // so a two-computer desk doesn't sprawl across a 5 x 3 field.
  int minR = m_rows, maxR = -1, minC = m_columns, maxC = -1;
  for (int cell = 0; cell < m_columns * m_rows; ++cell) {
    if (!occupied(cell) && !nextToOccupied(cell))
      continue;
    minR = std::min(minR, cell / m_columns);
    maxR = std::max(maxR, cell / m_columns);
    minC = std::min(minC, cell % m_columns);
    maxC = std::max(maxC, cell % m_columns);
  }
  if (maxR < 0)
    return;

  // Fit the visible columns to the window: shrink cards a little before scrolling.
  m_shownColumns = maxC - minC + 1;
  const int avail = std::max(0, width() - 176 - 48 - 16);
  const int perCol = (avail - (m_shownColumns - 1) * (kArrow + 12)) / std::max(1, m_shownColumns);
  m_cardW = std::clamp(perCol, kCardWMin, kCardWMax);

  for (int r = minR; r <= maxR; ++r) {
    for (int c = minC; c <= maxC; ++c) {
      const int cell = r * m_columns + c;
      const int gr = (r - minR) * 2;
      const int gc = (c - minC) * 2;
      if (occupied(cell)) {
        auto *card = makeCard(m_gridHost);
        const bool isMain = cell == m_mainCell;
        fillCard(card, m_cells.value(cell), isMain ? tr("Mouse & keyboard are here") : placeLabel(cell), isMain);
        m_grid->addWidget(card->frame, gr, gc, Qt::AlignCenter);
        delete card;
      } else if (nextToOccupied(cell)) {
        m_grid->addWidget(makeEmptyCell(cell, m_gridHost), gr, gc, Qt::AlignCenter);
      } else {
        spacer(gr, gc, m_cardW, kEmptyH);
      }
      // Arrows to the right and below when both cells hold computers.
      if (c < maxC) {
        const int right = cell + 1;
        if (occupied(cell) && occupied(right))
          arrow(Icon::ArrowRight, gr, gc + 1);
        else
          spacer(gr, gc + 1, kArrow, 4);
      }
      if (r < maxR) {
        const int below = cell + m_columns;
        if (occupied(cell) && occupied(below))
          arrow(Icon::ArrowDown, gr + 1, gc);
        else
          spacer(gr + 1, gc, 4, kArrow);
      }
    }
  }
}

void DeskView::setDesk(const QString &mainName, const ServerConfig &config)
{
  m_serverMode = true;
  m_mainName = mainName;
  m_columns = config.columns();
  m_rows = config.rows();
  m_cells.clear();

  const auto &list = config.computers();
  int mainIdx = -1;
  for (int i = 0; i < list.size(); ++i) {
    if (list[i].isNull())
      continue;
    if (list[i].name() == mainName)
      mainIdx = i;
    m_cells.insert(i, list[i].name());
  }
  if (mainIdx < 0) {
    // Not placed yet: the main computer lives in the centre.
    mainIdx = config.centreIndex();
    m_cells.insert(mainIdx, mainName);
  }
  m_mainCell = mainIdx;

  m_subtitle->setText(tr("Move the mouse off an edge to switch computers. Use ⋯ on a card to move or forget it."));
  m_addButton->setVisible(true);
  m_sharingPill->setVisible(true);
  relayout();
}

void DeskView::showDeskPage()
{
  m_navDesk->click();
}

void DeskView::showSettingsPage()
{
  m_navSettings->click();
}

void DeskView::showAboutPage()
{
  m_navHelp->click();
}

void DeskView::showPlanPage()
{
  // Plan lives under Settings in the rail.
  m_navSettings->setChecked(true);
  m_pages->setCurrentIndex(4);
}

void DeskView::showChecksPage()
{
  // Checks live under Help in the rail.
  m_navHelp->setChecked(true);
  m_pages->setCurrentIndex(5);
}

void DeskView::showComputerPage(const Computer &computer, const QString &status, bool linked)
{
  // A computer's page lives under Desk in the rail.
  m_navDesk->setChecked(true);
  int current = -1;
  QList<QPair<QString, int>> spots;
  for (int cell = 0; cell < m_columns * m_rows; ++cell) {
    if (m_cells.value(cell) == computer.name()) {
      current = cell;
      spots << qMakePair(placeLabel(cell), cell);
    } else if (!occupied(cell) && nextToOccupied(cell)) {
      spots << qMakePair(placeLabel(cell), cell);
    }
  }
  m_computer->showComputer(computer, spots, current, status, linked);
  m_pages->setCurrentIndex(6);
}

bool DeskView::onComputerPage() const
{
  return m_pages->currentIndex() == 6;
}

void DeskView::showPage(QWidget *page)
{
  m_navDesk->setChecked(true);
  if (m_pages->indexOf(page) < 0)
    m_pages->addWidget(page);
  m_pages->setCurrentWidget(page);
}

void DeskView::removePage(QWidget *page)
{
  if (m_pages->indexOf(page) >= 0)
    m_pages->removeWidget(page);
  if (m_pages->currentIndex() < 0 || m_pages->currentIndex() >= 7)
    showDeskPage();
}

NoticeCard *DeskView::notice()
{
  dismissNotice();
  m_notice = new NoticeCard(m_gridHost->parentWidget());
  m_noticeHost->addWidget(m_notice);
  showDeskPage();
  return m_notice;
}

bool DeskView::hasNotice() const
{
  return m_notice != nullptr;
}

void DeskView::dismissNotice()
{
  if (!m_notice)
    return;
  m_noticeHost->removeWidget(m_notice);
  m_notice->hide();
  m_notice->deleteLater();
  m_notice = nullptr;
}

void DeskView::setLinkedTo(const QString &thisName, const QString &mainName)
{
  m_serverMode = false;
  m_thisName = thisName;
  m_mainName = mainName;
  m_cells.clear();
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

void DeskView::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (!m_serverMode || m_shownColumns == 0)
    return;
  const int avail = std::max(0, width() - 176 - 48 - 16);
  const int perCol = (avail - (m_shownColumns - 1) * (kArrow + 12)) / std::max(1, m_shownColumns);
  if (std::clamp(perCol, kCardWMin, kCardWMax) != m_cardW)
    relayout();
}

bool DeskView::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::MouseButtonRelease) {
    const auto name = m_cardNames.value(watched);
    if (!name.isEmpty() && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
      if (m_serverMode && name != m_mainName)
        Q_EMIT computerClicked(name);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

} // namespace deskflow::gui
