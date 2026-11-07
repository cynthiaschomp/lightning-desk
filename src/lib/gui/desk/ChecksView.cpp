/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/ChecksView.h"

#include "desk/BrandIcons.h"
#include "gui/Logger.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTextCursor>
#include <QVBoxLayout>

using namespace deskflow::gui::brand;

namespace deskflow::gui {

namespace {

constexpr int kKeepRaw = 500;
constexpr int kKeepPlain = 200;

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

QPushButton *linkButton(const QString &t, QWidget *parent)
{
  auto *b = new QPushButton(t, parent);
  b->setCursor(Qt::PointingHandCursor);
  b->setFlat(true);
  b->setStyleSheet(QStringLiteral(
                       "QPushButton{background:transparent;color:%1;border:none;padding:0;font-size:12px;}"
                       "QPushButton:hover{color:#FFE352;}"
  )
                       .arg(kAccent));
  return b;
}

QLabel *statusDot(ChecksView::Status s, QWidget *parent)
{
  QString bg;
  QString glyph;
  switch (s) {
  case ChecksView::Status::Ok:
    bg = kGreen;
    glyph = QStringLiteral("✓");
    break;
  case ChecksView::Status::Warn:
    bg = kAccent;
    glyph = QStringLiteral("!");
    break;
  case ChecksView::Status::Bad:
    bg = kRed;
    glyph = QStringLiteral("!");
    break;
  case ChecksView::Status::Info:
    bg = kCard2;
    glyph = QStringLiteral("i");
    break;
  }
  auto *l = new QLabel(glyph, parent);
  l->setFixedSize(24, 24);
  l->setAlignment(Qt::AlignCenter);
  l->setStyleSheet(
      QStringLiteral("background:%1;color:%2;border-radius:12px;font-size:13px;font-weight:700;border:none;")
          .arg(bg, s == ChecksView::Status::Warn ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF"))
  );
  return l;
}

QFrame *card(const QString &name, QWidget *parent)
{
  auto *f = new QFrame(parent);
  f->setObjectName(name);
  f->setStyleSheet(QStringLiteral("QFrame#%1{background:%2;border-radius:16px;}QLabel{border:none;}").arg(name, kCard));
  return f;
}

} // namespace

ChecksView::ChecksView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("checksView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#checksView{background:%1;}").arg(kBg));

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *content = new QWidget(this);
  content->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(16);

  // Header
  auto *hdr = new QHBoxLayout();
  hdr->setSpacing(10);
  auto *back = button(tr("‹ Help"), false, content);
  connect(back, &QPushButton::clicked, this, &ChecksView::backRequested);
  hdr->addWidget(back, 0, Qt::AlignTop);
  auto *titles = new QVBoxLayout();
  titles->setSpacing(4);
  titles->addWidget(text(tr("Something not working?"), kText, 22, 700, content));
  titles->addWidget(text(
      tr("We run the usual checks and fix what we can. Anything left needs a hand from you."), kMuted, 13, 400, content
  ));
  hdr->addLayout(titles, 1);
  auto *rerun = button(tr("Run checks again"), false, content);
  connect(rerun, &QPushButton::clicked, this, &ChecksView::rerunRequested);
  hdr->addWidget(rerun, 0, Qt::AlignTop);
  col->addLayout(hdr);

  // Checks card
  auto *checks = card(QStringLiteral("checksCard"), content);
  m_checkRows = new QVBoxLayout(checks);
  m_checkRows->setContentsMargins(0, 0, 0, 0);
  m_checkRows->setSpacing(0);
  col->addWidget(checks);

  // Recent activity card
  auto *act = card(QStringLiteral("activityCard"), content);
  auto *av = new QVBoxLayout(act);
  av->setContentsMargins(0, 0, 0, 0);
  av->setSpacing(0);
  auto *ah = new QWidget(act);
  ah->setStyleSheet(QStringLiteral("background:transparent;border-bottom:1px solid %1;").arg(kCard2));
  auto *ahl = new QHBoxLayout(ah);
  ahl->setContentsMargins(16, 10, 12, 10);
  ahl->setSpacing(10);
  ahl->addWidget(text(tr("Recent activity"), kText, 13, 600, ah), 1);
  m_activityHint = text(tr("Plain language ·"), kDim, 12, 400, ah);
  ahl->addWidget(m_activityHint);
  m_rawToggle = linkButton(tr("show the raw log"), ah);
  connect(m_rawToggle, &QPushButton::clicked, this, [this] { setRaw(!m_showRaw); });
  ahl->addWidget(m_rawToggle);
  ahl->addSpacing(6);
  auto *copy = button(tr("Copy report"), false, ah);
  connect(copy, &QPushButton::clicked, this, &ChecksView::copyReportRequested);
  ahl->addWidget(copy);
  av->addWidget(ah);

  m_activity = new QPlainTextEdit(act);
  m_activity->setReadOnly(true);
  m_activity->setFrameShape(QFrame::NoFrame);
  m_activity->setMinimumHeight(150);
  m_activity->setPlaceholderText(tr("Nothing yet. Turn sharing on and this fills in as things happen."));
  m_activity->setStyleSheet(QStringLiteral(
                                "QPlainTextEdit{background:transparent;color:%1;border:none;padding:8px 12px;"
                                "font-size:13px;}"
                                "QScrollBar:vertical{background:transparent;width:8px;margin:0;}"
                                "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:24px;}"
                                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
  )
                                .arg(kMuted, kLine));
  av->addWidget(m_activity, 1);
  col->addWidget(act, 1);

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

  // The GUI's own log lines (the core's arrive through MainWindow).
  connect(Logger::instance(), &Logger::newLine, this, &ChecksView::appendLogLine);
}

void ChecksView::setChecks(const QList<Check> &checks)
{
  m_checks = checks;
  while (auto *item = m_checkRows->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }
  auto *host = m_checkRows->parentWidget();
  for (int i = 0; i < checks.size(); ++i) {
    const auto &c = checks.at(i);
    auto *row = new QWidget(host);
    row->setStyleSheet(
        i + 1 < checks.size() ? QStringLiteral("background:transparent;border-bottom:1px solid %1;").arg(kCard2)
                              : QStringLiteral("background:transparent;")
    );
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 11, 16, 11);
    h->setSpacing(14);
    auto *t = new QVBoxLayout();
    t->setSpacing(2);
    t->addWidget(text(c.title, kText, 15, 500, row));
    if (!c.detail.isEmpty())
      t->addWidget(text(c.detail, kDim, 12, 400, row));
    h->addLayout(t, 1);
    if (!c.badge.isEmpty()) {
      auto *b = new QLabel(c.badge, row);
      b->setStyleSheet(QStringLiteral(
                           "color:%1;background:rgba(255,214,10,0.14);border-radius:10px;padding:3px 9px;"
                           "font-size:11px;font-weight:600;border:none;"
      )
                           .arg(kAccent));
      h->addWidget(b);
    }
    if (!c.actionLabel.isEmpty()) {
      auto *a = button(c.actionLabel, c.status == Status::Bad, row);
      const auto id = c.actionId;
      connect(a, &QPushButton::clicked, this, [this, id] { Q_EMIT actionRequested(id); });
      h->addWidget(a);
    }
    h->addWidget(statusDot(c.status, row));
    m_checkRows->addWidget(row);
  }
}

void ChecksView::appendLogLine(const QString &line)
{
  m_raw << line;
  while (m_raw.size() > kKeepRaw)
    m_raw.removeFirst();
  const auto plain = plainFor(line);
  if (!plain.isEmpty()) {
    m_plain << plain;
    while (m_plain.size() > kKeepPlain)
      m_plain.removeFirst();
  }
  if (!isVisible())
    return; // refreshed on show
  if (m_showRaw)
    m_activity->appendPlainText(line);
  else if (!plain.isEmpty())
    m_activity->appendPlainText(plain);
}

void ChecksView::setRaw(bool raw)
{
  m_showRaw = raw;
  m_rawToggle->setText(raw ? tr("show plain language") : tr("show the raw log"));
  m_activityHint->setText(raw ? tr("Raw log ·") : tr("Plain language ·"));
  refreshActivity();
}

void ChecksView::refreshActivity()
{
  m_activity->setPlainText((m_showRaw ? m_raw : m_plain).join(QLatin1Char('\n')));
  m_activity->moveCursor(QTextCursor::End);
}

void ChecksView::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  refreshActivity();
}

QString ChecksView::report() const
{
  QStringList out;
  out << QStringLiteral("== Checks ==");
  for (const auto &c : m_checks) {
    QString mark;
    switch (c.status) {
    case Status::Ok:
      mark = QStringLiteral("OK  ");
      break;
    case Status::Warn:
      mark = QStringLiteral("WARN");
      break;
    case Status::Bad:
      mark = QStringLiteral("FAIL");
      break;
    case Status::Info:
      mark = QStringLiteral("info");
      break;
    }
    out << QStringLiteral("%1 %2 — %3").arg(mark, c.title, c.detail);
  }
  out << QString() << QStringLiteral("== Recent activity ==") << m_plain;
  out << QString() << QStringLiteral("== Raw log (last %1 lines) ==").arg(m_raw.size()) << m_raw;
  return out.join(QLatin1Char('\n'));
}

QString ChecksView::plainFor(const QString &line)
{
  // Lines look like: [2026-11-06T13:41:02.123] INFO: client "CYN-02" has connected
  static const QRegularExpression head(
      QStringLiteral(R"re(^\[(\d{4}-\d\d-\d\dT)?(\d\d:\d\d:\d\d)[^\]]*\]\s*(\w+):\s*(.*)$)re")
  );
  // Debug builds append "\n\tfile:line"; only the first line carries the message.
  const auto m = head.match(line.section(QLatin1Char('\n'), 0, 0));
  if (!m.hasMatch())
    return {};
  const auto when = m.captured(2);
  const auto level = m.captured(3);
  const auto msg = m.captured(4).trimmed();

  struct Rule
  {
    QRegularExpression re;
    QString out; // %1.. captures
  };
  static const QList<Rule> rules{
      {QRegularExpression(QStringLiteral(R"re(^client "([^"]+)" has connected)re")), tr("%1 linked up")},
      {QRegularExpression(QStringLiteral(R"re(^client "([^"]+)" has disconnected)re")), tr("%1 left the desk")},
      {QRegularExpression(QStringLiteral(R"re(^client "([^"]+)" is dead)re")), tr("%1 stopped answering")},
      {QRegularExpression(QStringLiteral(R"re(^client "([^"]+)" refused:)re")),
       tr("%1 asked to join but the plan is full")},
      {QRegularExpression(QStringLiteral(R"re(^server refused client with name "([^"]+)")re")),
       tr("The main computer turned this one away (%1)")},
      {QRegularExpression(QStringLiteral(R"re(^server already has a connected client with name "([^"]+)")re")),
       tr("Another computer is already using the name %1")},
      {QRegularExpression(QStringLiteral(R"re(^switch from "([^"]+)" to "([^"]+)")re")), tr("Moved from %1 to %2")},
      {QRegularExpression(QStringLiteral(R"re(^started server)re")), tr("Sharing started on this computer")},
      {QRegularExpression(QStringLiteral(R"re(^starting server process)re")), tr("Turning sharing on")},
      {QRegularExpression(QStringLiteral(R"re(^starting client process)re")), tr("Looking for the main computer")},
      {QRegularExpression(QStringLiteral(R"re(^sending start to daemon)re")),
       tr("Asking the background service to start")},
      {QRegularExpression(QStringLiteral(R"re(^stopping core desktop process)re")), tr("Stopping")},
      {QRegularExpression(QStringLiteral(R"re(^desktop process exited with code: (\d+))re")), tr("Stopped (code %1)")},
      {QRegularExpression(QStringLiteral(R"re(^accepted client connection)re")), tr("A computer is connecting")},
      {QRegularExpression(QStringLiteral(R"re(^stopped server)re")), tr("Sharing stopped")},
      {QRegularExpression(QStringLiteral(R"re(^started client)re")), tr("Looking for the main computer")},
      {QRegularExpression(QStringLiteral(R"re(^stopped client)re")), tr("Stopped looking for the main computer")},
      {QRegularExpression(QStringLiteral(R"re(^connected to server)re")), tr("Linked to the main computer")},
      {QRegularExpression(QStringLiteral(R"re(^disconnected from server)re")), tr("Lost the main computer")},
      {QRegularExpression(QStringLiteral(R"re(^server is dead)re")), tr("The main computer stopped answering")},
      {QRegularExpression(QStringLiteral(R"re(^failed to connect to server: (.*))re")),
       tr("Couldn't reach the main computer (%1)")},
      {QRegularExpression(QStringLiteral(R"re(^connected to secure socket)re")), tr("Connection is encrypted")},
      {QRegularExpression(QStringLiteral(R"re(^failed to connect secure socket)re")),
       tr("Encryption handshake failed")},
      {QRegularExpression(QStringLiteral(R"re(^scroll lock is on, locking cursor)re")),
       tr("Cursor locked to this computer (Scroll Lock)")},
      {QRegularExpression(QStringLiteral(R"re(^cursor is locked to computer)re")),
       tr("Cursor is locked to this computer")},
      {QRegularExpression(QStringLiteral(R"re(^server layouts missing on this computer: (.*))re")),
       tr("Keyboard layouts missing here: %1")},
      {QRegularExpression(QStringLiteral(R"re(^core stopped with error code: (\d+))re")),
       tr("Sharing stopped with an error (%1)")},
      {QRegularExpression(QStringLiteral(R"re(^daemon is stopping)re")), tr("Background service is stopping")},
  };
  for (const auto &r : rules) {
    const auto rm = r.re.match(msg);
    if (!rm.hasMatch())
      continue;
    QString out = r.out;
    for (int i = 1; i <= rm.lastCapturedIndex(); ++i)
      out = out.arg(rm.captured(i));
    return QStringLiteral("%1   %2").arg(when, out);
  }
  // Anything the core calls an error is worth a plain line even if we don't recognise it.
  if (level == QLatin1String("ERROR") || level == QLatin1String("FATAL"))
    return QStringLiteral("%1   %2 %3").arg(when, tr("Problem:"), msg);
  return {};
}

} // namespace deskflow::gui
