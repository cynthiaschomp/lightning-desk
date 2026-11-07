/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "desk/ShortcutsView.h"

#include "desk/BrandIcons.h"
#include "gui/widgets/KeySequenceWidget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QToolButton>
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

QWidget *keycaps(const QString &sequence, QWidget *parent)
{
  auto *w = new QWidget(parent);
  w->setStyleSheet(QStringLiteral("background:transparent;"));
  auto *h = new QHBoxLayout(w);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(4);
  const auto parts = sequence.split(QLatin1Char('+'), Qt::SkipEmptyParts);
  for (int i = 0; i < parts.size(); ++i) {
    auto *k = new QLabel(parts[i].trimmed(), w);
    k->setStyleSheet(QStringLiteral(
                         "color:#FFFFFF;background:%1;border:1px solid %2;border-bottom-width:2px;border-radius:6px;"
                         "padding:3px 8px;font-size:12px;font-weight:600;"
    )
                         .arg(kCard2, kLine));
    h->addWidget(k);
    if (i != parts.size() - 1)
      h->addWidget(text(QStringLiteral("+"), kDim, 12, 400, w));
  }
  return w;
}

Icon iconFor(const Hotkey &hotkey)
{
  if (hotkey.actions().isEmpty())
    return Icon::Keyboard;
  switch (static_cast<Action::Type>(hotkey.actions().first().type())) {
  case Action::Type::switchToComputer:
  case Action::Type::switchInDirection:
  case Action::Type::switchToNextComputer:
    return Icon::Laptop;
  case Action::Type::lockCursorToComputer:
    return Icon::Lock;
  default:
    return Icon::Keyboard;
  }
}

} // namespace

QPair<QString, QString> ShortcutsView::describe(const Hotkey &hotkey, const QString &mainName)
{
  if (hotkey.actions().isEmpty())
    return {hotkey.keySequence().toString(), QObject::tr("Does nothing yet")};
  const auto &a = hotkey.actions().first();
  switch (static_cast<Action::Type>(a.type())) {
  case Action::Type::switchToComputer:
    if (a.switchComputerName() == mainName)
      return {QObject::tr("Back to %1").arg(mainName), QObject::tr("Return to the main computer")};
    return {QObject::tr("Jump to %1").arg(a.switchComputerName()), QObject::tr("Moves the cursor there instantly")};
  case Action::Type::switchInDirection: {
    static const char *dirs[] = {
        QT_TRANSLATE_NOOP("deskflow::gui::ShortcutsView", "left"),
        QT_TRANSLATE_NOOP("deskflow::gui::ShortcutsView", "right"),
        QT_TRANSLATE_NOOP("deskflow::gui::ShortcutsView", "up"),
        QT_TRANSLATE_NOOP("deskflow::gui::ShortcutsView", "down")
    };
    const int d = qBound(0, a.switchDirection(), 3);
    return {QObject::tr("Switch %1").arg(tr(dirs[d])), QObject::tr("Moves the cursor to the next computer that way")};
  }
  case Action::Type::switchToNextComputer:
    return {QObject::tr("Next computer"), QObject::tr("Cycles through the computers on your desk")};
  case Action::Type::lockCursorToComputer:
    return {
        QObject::tr("Stay on this computer"), QObject::tr("Stops the mouse crossing edges until you press it again")
    };
  default:
    return {hotkey.text(), QObject::tr("Advanced shortcut")};
  }
}

ShortcutsView::ShortcutsView(QWidget *parent) : QWidget(parent)
{
  setObjectName(QStringLiteral("shortcutsView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("QWidget#shortcutsView{background:%1;}").arg(kBg));
  auto *col = new QVBoxLayout(this);
  col->setContentsMargins(24, 22, 24, 22);
  col->setSpacing(18);

  auto *hdr = new QHBoxLayout();
  auto *titles = new QVBoxLayout();
  titles->setSpacing(4);
  titles->addWidget(text(tr("Shortcuts"), kText, 22, 700, this));
  titles->addWidget(text(tr("Jump between computers or pause sharing from the keyboard."), kDim, 13, 400, this));
  hdr->addLayout(titles, 1);
  auto *add = new QPushButton(tr("+ New shortcut"), this);
  add->setCursor(Qt::PointingHandCursor);
  add->setStyleSheet(QStringLiteral(
                         "QPushButton{color:%1;background:%2;border:none;border-radius:14px;padding:6px 16px;"
                         "font-size:12px;font-weight:700;}QPushButton:hover{background:#FFE14D;}"
  )
                         .arg(kBg, kAccent));
  connect(add, &QPushButton::clicked, this, [this] {
    NewShortcutDialog dialog(this, m_computers, m_mainName);
    if (dialog.exec() == QDialog::Accepted)
      Q_EMIT addRequested(dialog.hotkey());
  });
  hdr->addWidget(add, 0, Qt::AlignTop);
  col->addLayout(hdr);

  m_listHost = new QWidget(this);
  m_listHost->setStyleSheet(QStringLiteral("background:transparent;"));
  m_list = new QVBoxLayout(m_listHost);
  m_list->setContentsMargins(0, 0, 0, 0);
  m_list->setSpacing(0);

  auto *scroll = new QScrollArea(this);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setWidget(m_listHost);
  scroll->setStyleSheet(QStringLiteral("QScrollArea{background:%1;border:none;}").arg(kBg));
  col->addWidget(scroll, 1);

  m_empty = text(
      tr("No shortcuts yet. Add one to jump straight to a computer, or to keep the mouse on this screen."), kDim, 13,
      400, this
  );
  col->addWidget(m_empty);

  auto *tip = new QHBoxLayout();
  auto *help = new QLabel(this);
  help->setPixmap(icon(Icon::Help, 16, devicePixelRatioF(), kDim));
  help->setStyleSheet(QStringLiteral("background:transparent;"));
  tip->addWidget(help);
  tip->addWidget(
      text(tr("Shortcuts work on every computer in your desk, whichever one you're typing on."), kDim, 13, 400, this), 1
  );
  col->addLayout(tip);
}

void ShortcutsView::setShortcuts(const HotkeyList &hotkeys, const QStringList &computers, const QString &mainName)
{
  m_hotkeys = hotkeys;
  m_computers = computers;
  m_mainName = mainName;
  rebuild();
}

void ShortcutsView::rebuild()
{
  while (auto *item = m_list->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }
  for (int i = 0; i < m_hotkeys.size(); ++i)
    m_list->addWidget(makeRow(i, m_hotkeys[i], i == m_hotkeys.size() - 1));
  m_list->addStretch(1);
  m_empty->setVisible(m_hotkeys.isEmpty());
}

QWidget *ShortcutsView::makeRow(int index, const Hotkey &hotkey, bool last)
{
  auto *row = new QFrame(m_listHost);
  row->setObjectName(QStringLiteral("scRow"));
  const QString radius = m_hotkeys.size() == 1 ? QStringLiteral("border-radius:12px;")
                         : index == 0 ? QStringLiteral("border-top-left-radius:12px;border-top-right-radius:12px;")
                         : last ? QStringLiteral("border-bottom-left-radius:12px;border-bottom-right-radius:12px;")
                                : QString();
  row->setStyleSheet(QStringLiteral("QFrame#scRow{background:%1;border:1px solid %2;%3%4}QLabel{border:none;}")
                         .arg(kCard, kLine, radius, last ? QString() : QStringLiteral("border-bottom:none;")));
  auto *h = new QHBoxLayout(row);
  h->setContentsMargins(16, 12, 12, 12);
  h->setSpacing(14);

  auto *ic = new QLabel(row);
  ic->setPixmap(tile(iconFor(hotkey), 36, devicePixelRatioF(), kCard2, kAccent));
  h->addWidget(ic);

  const auto [title, sub] = describe(hotkey, m_mainName);
  auto *txt = new QVBoxLayout();
  txt->setSpacing(2);
  txt->addWidget(text(title, kText, 14, 600, row));
  txt->addWidget(text(sub, kDim, 12, 400, row));
  h->addLayout(txt, 1);

  h->addWidget(keycaps(hotkey.keySequence().toString(), row));

  auto *remove = new QToolButton(row);
  remove->setText(QStringLiteral("✕"));
  remove->setToolTip(tr("Remove this shortcut"));
  remove->setCursor(Qt::PointingHandCursor);
  remove->setFixedSize(26, 26);
  remove->setStyleSheet(QStringLiteral(
                            "QToolButton{color:%1;background:transparent;border:none;border-radius:6px;font-size:12px;}"
                            "QToolButton:hover{background:%2;color:%3;}"
  )
                            .arg(kDim, kCard2, kRed));
  connect(remove, &QToolButton::clicked, this, [this, index] { Q_EMIT removeRequested(index); });
  h->addSpacing(6);
  h->addWidget(remove);
  return row;
}

// ----------------------------------------------------------------------------

NewShortcutDialog::NewShortcutDialog(QWidget *parent, const QStringList &computers, const QString &mainName)
    : QDialog(parent)
{
  setModal(true);
  setWindowTitle(QStringLiteral("Lightning Desk"));
  setFixedWidth(520);
  setStyleSheet(
      QStringLiteral(
          "QDialog{background:#111112;}QLabel{color:#FFFFFF;background:transparent;}"
          "QRadioButton{color:#FFFFFF;font-size:14px;spacing:10px;padding:8px 4px;}"
          "QRadioButton::indicator{width:18px;height:18px;border-radius:9px;border:2px solid "
          "%1;background:transparent;}"
          "QRadioButton::indicator:checked{border-color:%2;background:qradialgradient(cx:0.5,cy:0.5,radius:0.5,"
          "fx:0.5,fy:0.5,stop:0 %2,stop:0.45 %2,stop:0.5 transparent);}"
          "QComboBox{color:#FFFFFF;background:%3;border:1px solid %1;border-radius:9px;padding:6px 10px;font-size:13px;"
          "min-width:160px;}QComboBox QAbstractItemView{background:%3;color:#FFFFFF;selection-background-color:%4;}"
          "QPushButton{border:none;border-radius:12px;padding:10px 18px;font-size:14px;font-weight:600;}"
          "QPushButton#primary{background:%2;color:#000000;}QPushButton#primary:disabled{background:%4;color:%5;}"
          "QPushButton#primary:hover:enabled{background:#FFE14D;}"
          "QPushButton#secondary{background:%4;color:#FFFFFF;}"
          "KeySequenceWidget{color:#FFFFFF;background:%3;border:1px dashed %1;border-radius:12px;padding:14px;"
          "font-size:15px;font-weight:600;text-align:center;}"
          "KeySequenceWidget:pressed,KeySequenceWidget:checked{border:1px solid %2;color:%2;}"
      )
          .arg(kLine, kAccent, kCard, kCard2, kFaint)
  );

  auto *v = new QVBoxLayout(this);
  v->setContentsMargins(26, 26, 26, 22);
  v->setSpacing(14);

  v->addWidget(text(tr("New shortcut"), kText, 20, 700, this));
  v->addWidget(text(tr("1. What should it do?"), kDim, 12, 700, this));

  auto *group = new QButtonGroup(this);
  auto *jump = new QRadioButton(tr("Jump to a computer"), this);
  auto *pick = new QComboBox(this);
  for (const auto &c : computers)
    if (c != mainName)
      pick->addItem(c);
  auto *jumpRow = new QHBoxLayout();
  jumpRow->addWidget(jump);
  jumpRow->addWidget(pick);
  jumpRow->addStretch();
  auto *back = new QRadioButton(tr("Back to %1 (the main computer)").arg(mainName), this);
  auto *stay = new QRadioButton(tr("Stay on this computer until I press it again"), this);
  auto *next = new QRadioButton(tr("Next computer"), this);
  group->addButton(jump, 0);
  group->addButton(back, 1);
  group->addButton(stay, 2);
  group->addButton(next, 3);
  v->addLayout(jumpRow);
  v->addWidget(back);
  v->addWidget(stay);
  v->addWidget(next);
  (pick->count() > 0 ? jump : stay)->setChecked(true);
  jump->setEnabled(pick->count() > 0);
  pick->setEnabled(pick->count() > 0);

  v->addSpacing(6);
  v->addWidget(text(tr("2. Press the keys"), kDim, 12, 700, this));
  auto *keys = new KeySequenceWidget(this);
  keys->setKeyPrefix(QString());
  keys->setKeyPostfix(QString());
  keys->setMousePrefix(tr("Mouse button "));
  keys->setMousePostfix(QString());
  keys->setText(tr("Click here, then press the keys you want"));
  keys->setMinimumHeight(56);
  keys->setCursor(Qt::PointingHandCursor);
  v->addWidget(keys);
  auto *hint = text(tr("Use a modifier such as Ctrl or Alt so it doesn't fire while you type."), kDim, 12, 400, this);
  v->addWidget(hint);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *cancel = new QPushButton(tr("Cancel"), this);
  cancel->setObjectName(QStringLiteral("secondary"));
  auto *save = new QPushButton(tr("Add shortcut"), this);
  save->setObjectName(QStringLiteral("primary"));
  save->setEnabled(false);
  buttons->addWidget(cancel);
  buttons->addWidget(save);
  v->addSpacing(4);
  v->addLayout(buttons);

  connect(keys, &KeySequenceWidget::keySequenceChanged, this, [keys, save] { save->setEnabled(keys->valid()); });
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(save, &QPushButton::clicked, this, [this, group, pick, keys, mainName] {
    Action action;
    switch (group->checkedId()) {
    case 0:
      action.setType(static_cast<int>(Action::Type::switchToComputer));
      action.setSwitchComputerName(pick->currentText());
      break;
    case 1:
      action.setType(static_cast<int>(Action::Type::switchToComputer));
      action.setSwitchComputerName(mainName);
      break;
    case 2:
      action.setType(static_cast<int>(Action::Type::lockCursorToComputer));
      action.setLockCursorMode(static_cast<int>(Action::LockCursorMode::toggle));
      break;
    default:
      action.setType(static_cast<int>(Action::Type::switchToNextComputer));
      break;
    }
    m_hotkey = Hotkey();
    m_hotkey.setKeySequence(keys->keySequence());
    m_hotkey.addAction(action);
    accept();
  });
}

} // namespace deskflow::gui
