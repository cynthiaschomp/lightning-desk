/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/ComputerView.h"

#include "desk/BrandIcons.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
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

QPushButton *dangerButton(const QString &t, QWidget *parent)
{
  auto *b = new QPushButton(t, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{background:rgba(255,69,58,0.14);color:%1;border:none;border-radius:11px;"
                       "padding:8px 16px;font-size:13px;font-weight:600;}"
                       "QPushButton:hover{background:rgba(255,69,58,0.24);}"
  )
                       .arg(kRed));
  return b;
}

QPushButton *makeSwitch(QWidget *parent)
{
  auto *b = new QPushButton(parent);
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{min-width:44px;max-width:44px;min-height:24px;max-height:24px;"
                       "border-radius:12px;background:%1;border:none;color:transparent;}"
                       "QPushButton:checked{background:%2;}QPushButton:disabled{background:%3;}"
  )
                       .arg(kLine, kGreen, kCard2));
  return b;
}

QFrame *row(const QString &title, const QString &sub, QWidget *control, QWidget *parent)
{
  auto *r = new QFrame(parent);
  r->setObjectName(QStringLiteral("cRow"));
  auto *h = new QHBoxLayout(r);
  h->setContentsMargins(16, 11, 14, 11);
  h->setSpacing(14);
  auto *txt = new QVBoxLayout();
  txt->setSpacing(2);
  txt->addWidget(text(title, kText, 13, 600, r));
  if (!sub.isEmpty())
    txt->addWidget(text(sub, kDim, 11, 400, r));
  h->addLayout(txt, 1);
  if (control) {
    control->setParent(r);
    h->addWidget(control, 0, Qt::AlignVCenter);
  }
  return r;
}

QWidget *section(const QString &title, const QList<QFrame *> &rows, QWidget *parent)
{
  auto *w = new QWidget(parent);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  auto *l = text(title.toUpper(), kDim, 11, 700, w);
  l->setContentsMargins(2, 0, 0, 8);
  v->addWidget(l);
  for (int i = 0; i < rows.size(); ++i) {
    auto *r = rows[i];
    const bool first = i == 0;
    const bool last = i == rows.size() - 1;
    const QString radius = (first && last) ? QStringLiteral("border-radius:12px;")
                           : first         ? QStringLiteral("border-top-left-radius:12px;border-top-right-radius:12px;")
                           : last ? QStringLiteral("border-bottom-left-radius:12px;border-bottom-right-radius:12px;")
                                  : QString();
    r->setStyleSheet(QStringLiteral("QFrame#cRow{background:%1;border:1px solid %2;%3%4}QLabel{border:none;}")
                         .arg(kCard, kLine, radius, last ? QString() : QStringLiteral("border-bottom:none;")));
    v->addWidget(r);
  }
  return w;
}

QString pillStyle(const QString &fg, const QString &bg)
{
  return QStringLiteral(
             "color:%1;background:%2;border-radius:10px;padding:3px 10px;font-size:11px;font-weight:700;"
             "border:none;"
  )
      .arg(fg, bg);
}

} // namespace

ComputerView::ComputerView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("computerView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#computerView{background:%1;}").arg(kBg));

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);

  // Header: back, icon, name + subtitle, status pill
  auto *hdr = new QHBoxLayout();
  hdr->setSpacing(12);
  auto *back = button(tr("‹ Desk"), false, content);
  connect(back, &QPushButton::clicked, this, &ComputerView::backRequested);
  hdr->addWidget(back, 0, Qt::AlignVCenter);
  auto *ic = new QLabel(content);
  ic->setPixmap(tile(Icon::Monitor, 40, devicePixelRatioF(), kCard2, kText));
  ic->setStyleSheet(QStringLiteral("background:transparent;"));
  hdr->addWidget(ic);
  auto *tt = new QVBoxLayout();
  tt->setSpacing(2);
  m_title = text(QString(), kText, 20, 700, content);
  m_subtitle = text(QString(), kDim, 12, 400, content);
  tt->addWidget(m_title);
  tt->addWidget(m_subtitle);
  hdr->addLayout(tt, 1);
  m_pill = new QLabel(content);
  hdr->addWidget(m_pill, 0, Qt::AlignVCenter);
  col->addLayout(hdr);

  // Where it sits
  {
    auto *host = new QFrame(content);
    host->setObjectName(QStringLiteral("spots"));
    host->setStyleSheet(QStringLiteral("QFrame#spots{background:%1;border-radius:12px;}").arg(kCard));
    m_spotRow = new QGridLayout(host);
    m_spotRow->setContentsMargins(4, 4, 4, 4);
    m_spotRow->setHorizontalSpacing(4);
    m_spotRow->setVerticalSpacing(4);
    for (int c = 0; c < 4; ++c)
      m_spotRow->setColumnStretch(c, 1);
    m_spotHost = host;
    auto *w = new QWidget(content);
    w->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *l = text(tr("Where it sits").toUpper(), kDim, 11, 700, w);
    l->setContentsMargins(2, 0, 0, 8);
    v->addWidget(l);
    v->addWidget(host);
    v->addWidget(text(tr("Pick a spot next to a computer that's already on the desk."), kFaint, 11, 400, w));
    col->addWidget(w);
  }

  // Also answers to
  {
    auto *host = new QFrame(content);
    host->setObjectName(QStringLiteral("aliases"));
    host->setStyleSheet(
        QStringLiteral("QFrame#aliases{background:%1;border:1px solid %2;border-radius:12px;}QLabel{border:none;}")
            .arg(kCard, kLine)
    );
    auto *hv = new QVBoxLayout(host);
    hv->setContentsMargins(12, 10, 12, 10);
    hv->setSpacing(8);
    m_aliasRow = new QHBoxLayout();
    m_aliasRow->setSpacing(8);
    hv->addLayout(m_aliasRow);
    auto *addRow = new QHBoxLayout();
    addRow->setSpacing(8);
    m_aliasEdit = new QLineEdit(host);
    m_aliasEdit->setPlaceholderText(tr("Another name this computer uses (its hostname, for example)"));
    m_aliasEdit->setStyleSheet(QStringLiteral(
                                   "QLineEdit{color:#FFFFFF;background:%1;border:1px solid %2;"
                                   "border-radius:9px;padding:7px 10px;font-size:13px;}"
    )
                                   .arg(kCard2, kLine));
    m_aliasAdd = button(tr("Add name"), false, host);
    addRow->addWidget(m_aliasEdit, 1);
    addRow->addWidget(m_aliasAdd);
    hv->addLayout(addRow);
    m_aliasHost = host;
    auto add = [this] {
      const auto name = m_aliasEdit->text().trimmed();
      if (name.isEmpty() || name == m_computer.name() || m_computer.aliases().contains(name))
        return;
      m_computer.addAlias(name);
      m_aliasEdit->clear();
      rebuildAliases();
      pushChange();
    };
    connect(m_aliasAdd, &QPushButton::clicked, this, add);
    connect(m_aliasEdit, &QLineEdit::returnPressed, this, add);

    auto *w = new QWidget(content);
    w->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *l = text(tr("Also answers to").toUpper(), kDim, 11, 700, w);
    l->setContentsMargins(2, 0, 0, 8);
    v->addWidget(l);
    v->addWidget(host);
    col->addWidget(w);
  }

  // Key fixes (half-duplex lock keys) and the X11 multi-monitor fix
  m_lockSync = makeSwitch(content);
  m_xtest = makeSwitch(content);
  col->addWidget(section(
      tr("Key fixes"),
      {row(tr("Caps, Num and Scroll Lock get stuck or flip twice"),
           tr("Turn on if a lock key on this computer only sends one signal (some Mac keyboards do)"), m_lockSync,
           content),
       row(tr("Mouse lands on the wrong monitor (Linux X11)"), tr("Only for old X11 setups with several monitors"),
           m_xtest, content)},
      content
  ));

  // Corners
  m_corners = makeSwitch(content);
  m_cornerSize = new QSpinBox(content);
  m_cornerSize->setRange(0, 999);
  m_cornerSize->setSuffix(QStringLiteral(" px"));
  m_cornerSize->setStyleSheet(QStringLiteral(
                                  "QSpinBox{color:#FFFFFF;background:%1;border:1px solid %2;border-radius:9px;"
                                  "padding:5px 10px;font-size:13px;min-width:70px;}"
  )
                                  .arg(kCard2, kLine));
  col->addWidget(section(
      tr("Switching"),
      {row(tr("Don't switch in the corners"),
           tr("Lets you reach close buttons and menus in the corners without jumping computers"), m_corners, content),
       row(tr("Corner size"), QString(), m_cornerSize, content)},
      content
  ));

  // Details (from the connection log)
  {
    QList<QFrame *> rows;
    for (const auto &title :
         {tr("Address"), tr("Screen"), tr("First linked"), tr("Last linked"), tr("Linked"), tr("Security code"),
          tr("Protocol")}) {
      auto *value = text(QStringLiteral("—"), kDim, 12, 400, content);
      value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      value->setTextInteractionFlags(Qt::TextSelectableByMouse);
      auto *r = row(title, QString(), value, content);
      m_detailRows << qMakePair(static_cast<QLabel *>(nullptr), value);
      rows << r;
    }
    m_details = section(tr("Details"), rows, content);
    col->addWidget(m_details);
  }

  // Footer
  auto *foot = new QHBoxLayout();
  auto *forget = dangerButton(tr("Forget this computer"), content);
  connect(forget, &QPushButton::clicked, this, [this] { Q_EMIT forgetRequested(m_computer.name()); });
  foot->addWidget(forget);
  foot->addStretch();
  col->addLayout(foot);
  col->addStretch(1);

  connect(m_lockSync, &QPushButton::toggled, this, [this](bool on) {
    if (m_loading)
      return;
    m_computer.setFix(ComputerConfig::Fix::CapsLock, on);
    m_computer.setFix(ComputerConfig::Fix::NumLock, on);
    m_computer.setFix(ComputerConfig::Fix::ScrollLock, on);
    pushChange();
  });
  connect(m_xtest, &QPushButton::toggled, this, [this](bool on) {
    if (m_loading)
      return;
    m_computer.setFix(ComputerConfig::Fix::XTest, on);
    pushChange();
  });
  connect(m_corners, &QPushButton::toggled, this, [this](bool on) {
    if (m_loading)
      return;
    for (auto c :
         {ComputerConfig::SwitchCorner::TopLeft, ComputerConfig::SwitchCorner::TopRight,
          ComputerConfig::SwitchCorner::BottomLeft, ComputerConfig::SwitchCorner::BottomRight})
      m_computer.setSwitchCorner(c, on);
    m_cornerSize->setEnabled(on);
    pushChange();
  });
  connect(m_cornerSize, &QSpinBox::valueChanged, this, [this](int v) {
    if (m_loading)
      return;
    m_computer.setSwitchCornerSize(v);
    pushChange();
  });

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

void ComputerView::showComputer(
    const Computer &computer, const QList<QPair<QString, int>> &spots, int currentCell, const QString &status,
    bool linked
)
{
  m_loading = true;
  m_computer = computer;
  m_spots = spots;
  m_currentCell = currentCell;

  m_title->setText(computer.name());
  m_subtitle->setText(
      computer.aliases().isEmpty() ? tr("On your desk")
                                   : tr("Also %1").arg(computer.aliases().join(QStringLiteral(", ")))
  );
  m_pill->setText(status);
  m_pill->setStyleSheet(linked ? pillStyle(kGreen, QStringLiteral("rgba(52,199,89,0.14)")) : pillStyle(kDim, kCard2));

  rebuildSpots();
  rebuildAliases();

  const bool lock = computer.fix(ComputerConfig::Fix::CapsLock) || computer.fix(ComputerConfig::Fix::NumLock) ||
                    computer.fix(ComputerConfig::Fix::ScrollLock);
  m_lockSync->setChecked(lock);
  m_xtest->setChecked(computer.fix(ComputerConfig::Fix::XTest));
  const bool corners =
      computer.switchCorner(0) || computer.switchCorner(1) || computer.switchCorner(2) || computer.switchCorner(3);
  m_corners->setChecked(corners);
  m_cornerSize->setValue(computer.switchCornerSize());
  m_cornerSize->setEnabled(corners);
  m_loading = false;
}

void ComputerView::setDetails(const ComputerLog::Entry &e)
{
  const bool known = !e.name.isEmpty();
  m_details->setVisible(true);
  auto fmt = [](const QDateTime &t) {
    return t.isValid() ? t.toString(QStringLiteral("MMM d, h:mm AP")) : QStringLiteral("—");
  };
  QStringList values;
  values
      << (e.address.isEmpty()
              ? QStringLiteral("—")
              : (e.addresses.size() > 1
                     ? tr("%1 (before: %2)").arg(e.address, e.addresses.mid(1).join(QStringLiteral(", ")))
                     : e.address));
  values << (e.screen.isEmpty() ? QStringLiteral("—") : e.screen);
  values << fmt(e.firstSeen);
  values << (e.linkedNow ? tr("Now") : fmt(e.lastLeft.isValid() ? e.lastLeft : e.lastSeen));
  values
      << (known ? tr("%n time(s) · %1 h %2 min in total", "", e.timesLinked)
                      .arg(e.totalSeconds / 3600)
                      .arg((e.totalSeconds % 3600) / 60)
                : tr("Never"));
  values << (e.fingerprint.isEmpty() ? QStringLiteral("—") : e.fingerprint);
  values << (e.protocol.isEmpty() ? QStringLiteral("—") : e.protocol);
  for (int i = 0; i < m_detailRows.size() && i < values.size(); ++i)
    m_detailRows[i].second->setText(values[i]);
}

void ComputerView::rebuildSpots()
{
  while (auto *item = m_spotRow->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }
  int n = 0;
  auto seg = [this, &n](const QString &label, bool current) {
    auto *b = new QPushButton(label, m_spotHost);
    b->setCursor(current ? Qt::ArrowCursor : Qt::PointingHandCursor);
    b->setStyleSheet(QStringLiteral(
                         "QPushButton{background:%1;color:%2;border:none;border-radius:9px;padding:9px 10px;"
                         "font-size:12px;font-weight:600;min-width:0;}QPushButton:hover{background:%3;}"
    )
                         .arg(
                             current ? kAccent : QStringLiteral("transparent"),
                             current ? QStringLiteral("#000000") : kText, current ? kAccent : kCard2
                         ));
    b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_spotRow->addWidget(b, n / 4, n % 4);
    ++n;
    return b;
  };
  // Current spot first, highlighted; then the free ones.
  QString here;
  for (const auto &[label, cell] : m_spots)
    if (cell == m_currentCell)
      here = label;
  seg(here.isEmpty() ? tr("Here") : here, true);
  for (const auto &[label, cell] : m_spots) {
    if (cell == m_currentCell)
      continue;
    auto *b = seg(label, false);
    const auto name = m_computer.name();
    connect(b, &QPushButton::clicked, this, [this, name, cell] { Q_EMIT moveRequested(name, cell); });
  }
}

void ComputerView::rebuildAliases()
{
  while (auto *item = m_aliasRow->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }
  if (m_computer.aliases().isEmpty()) {
    m_aliasRow->addWidget(text(tr("No other names yet."), kFaint, 12, 400, m_aliasHost));
  }
  for (const auto &alias : m_computer.aliases()) {
    auto *chip = new QPushButton(QStringLiteral("%1  ✕").arg(alias), m_aliasHost);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setToolTip(tr("Remove %1").arg(alias));
    chip->setStyleSheet(QStringLiteral(
                            "QPushButton{background:%1;color:%2;border:none;border-radius:12px;padding:5px 12px;"
                            "font-size:12px;font-weight:500;}QPushButton:hover{background:%3;color:%4;}"
    )
                            .arg(kCard2, kText, kLine, kRed));
    connect(chip, &QPushButton::clicked, this, [this, alias] {
      m_computer.aliases().removeAll(alias);
      rebuildAliases();
      pushChange();
    });
    m_aliasRow->addWidget(chip);
  }
  m_aliasRow->addStretch();
  m_subtitle->setText(
      m_computer.aliases().isEmpty() ? tr("On your desk")
                                     : tr("Also %1").arg(m_computer.aliases().join(QStringLiteral(", ")))
  );
}

void ComputerView::pushChange()
{
  if (!m_loading)
    Q_EMIT computerChanged(m_computer);
}

} // namespace deskflow::gui
