/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "desk/BrandIcons.h"

#include <QFrame>
#include <QList>
#include <QString>
#include <QStringList>

class QVBoxLayout;
class QButtonGroup;
class QLabel;

namespace deskflow::gui {

/**
 * @brief An inline notice that slides in at the top of a page instead of a dialog: icon tile,
 * title, body, optional security code, optional one-of-N choice, and up to three buttons.
 * Nothing in Lightning Desk pops over anything else; this is how the app asks a question.
 */
class NoticeCard : public QFrame
{
  Q_OBJECT
public:
  struct Choice
  {
    QString label;
    int value = 0;
    bool enabled = true;
  };
  enum class Tone
  {
    Neutral,
    Accent,
    Danger
  };

  explicit NoticeCard(QWidget *parent = nullptr);

  NoticeCard *setIcon(brand::Icon icon, Tone tone = Tone::Accent);
  NoticeCard *setTitle(const QString &title);
  NoticeCard *setBody(const QString &body);
  NoticeCard *setCode(const QString &hex); ///< shown as four 4-character groups
  NoticeCard *setChoices(const QList<Choice> &choices, int selectedValue = -1);
  NoticeCard *setFootnote(const QString &note); ///< small line with a lock icon
  /// Buttons left to right; the last one is the primary (yellow). A danger primary is red.
  NoticeCard *setButtons(const QStringList &labels, bool primaryIsDanger = false);

  int chosenValue() const;

Q_SIGNALS:
  /// @p button is the index into the labels passed to setButtons.
  void answered(int button, int chosenValue);

private:
  QVBoxLayout *m_body = nullptr;
  QWidget *m_iconHost = nullptr;
  QLabel *m_icon = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_text = nullptr;
  QWidget *m_codeRow = nullptr;
  QWidget *m_choiceRow = nullptr;
  QButtonGroup *m_choices = nullptr;
  QWidget *m_noteRow = nullptr;
  QLabel *m_note = nullptr;
  QWidget *m_buttonRow = nullptr;
};

} // namespace deskflow::gui
