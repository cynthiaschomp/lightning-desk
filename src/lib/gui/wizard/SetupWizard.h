// SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
// SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QButtonGroup;
class QVBoxLayout;

namespace deskflow::gui {

/**
 * @brief Lightning Desk first-run setup wizard.
 *
 * Seven plain-language steps (Welcome, Which computer, Name, Link,
 * Arrange, Checks, Done). Never shows IPs, ports, fingerprints or the
 * words "server" / "client". Writes the same Settings the main window
 * uses, so finishing the wizard leaves a working configuration.
 */
class SetupWizard : public QDialog
{
  Q_OBJECT

public:
  explicit SetupWizard(QWidget *parent = nullptr);
  ~SetupWizard() override = default;

  /// True when the wizard has never been completed on this machine.
  static bool isFirstRun();

  /// Re-enter at the "Link your other computers" step.
  void openAtLink();

  enum class Role
  {
    Main,
    Extra
  };
  enum class Side
  {
    Above,
    Left,
    Right,
    Below
  };

Q_SIGNALS:
  void finishedSetup(Role role);

private Q_SLOTS:
  void next();
  void back();

private:
  enum Step
  {
    Welcome = 0,
    WhichComputer,
    Name,
    Link,
    Arrange,
    Checks,
    Done,
    StepCount
  };

  void buildUi();
  QWidget *buildWelcome();
  QWidget *buildWhichComputer();
  QWidget *buildName();
  QWidget *buildLink();
  QWidget *buildArrange();
  QWidget *buildChecks();
  QWidget *buildDone();

  QWidget *card(QWidget *parent = nullptr) const;
  QLabel *title(const QString &text, QWidget *parent = nullptr) const;
  QLabel *body(const QString &text, QWidget *parent = nullptr) const;
  QPushButton *choiceButton(const QString &heading, const QString &detail, const QString &hint) const;
  QPushButton *sideButton(const QString &text, Side side);

  void goTo(int step);
  void updateChrome();
  void applyStep(int step);
  void runChecks();
  void addCheckRow(const QString &text, const QString &sub, bool fixed);
  int gridIndexFor(Side side) const;

  QStackedWidget *m_pages = nullptr;
  QProgressBar *m_progress = nullptr;
  QLabel *m_stepLabel = nullptr;
  QLabel *m_stepName = nullptr;
  QPushButton *m_back = nullptr;
  QPushButton *m_next = nullptr;

  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_linkNameEdit = nullptr;
  QLabel *m_arrangeIntro = nullptr;
  QLabel *m_mainTile = nullptr;
  QLabel *m_doneHint = nullptr;
  QVBoxLayout *m_checksLayout = nullptr;
  QButtonGroup *m_sideGroup = nullptr;
  QPushButton *m_mainChoice = nullptr;
  QPushButton *m_extraChoice = nullptr;

  Role m_role = Role::Main;
  Side m_side = Side::Right;
  QString m_name;
  QString m_otherName;
};

} // namespace deskflow::gui
