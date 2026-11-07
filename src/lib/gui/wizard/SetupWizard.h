// SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
// SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#pragma once

#include <QDialog>
#include <QMap>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include <memory>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QButtonGroup;
class QTimer;
class QVBoxLayout;
class ServerConfig;

namespace deskflow::gui {

class CoreProcess;

/**
 * @brief Lightning Desk first-run setup wizard.
 *
 * Seven plain-language steps (Welcome, Which computer, Name, Link,
 * Arrange, Checks, Done). Never shows the words "server" / "client" and
 * keeps addresses and fingerprints out of the normal path. While the Link
 * step is open the wizard runs the core itself, so computers appear in a
 * live list as they connect, and an extra screen gets a real
 * connected / failed result with troubleshooting text.
 */
class SetupWizard : public QDialog
{
  Q_OBJECT

public:
  explicit SetupWizard(QWidget *parent = nullptr);
  ~SetupWizard() override;

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

public Q_SLOTS:
  void done(int result) override;

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

  enum class ExtraState
  {
    Idle,
    Connecting,
    Connected,
    Reconnecting,
    Failed
  };

  // Layout helpers
  void buildUi();
  QWidget *buildWelcome();
  QWidget *buildWhichComputer();
  QWidget *buildName();
  QWidget *buildLink();
  QWidget *buildLinkMain();
  QWidget *buildLinkExtra();
  QWidget *buildArrange();
  QWidget *buildChecks();
  QWidget *buildDone();

  QWidget *card(QWidget *parent = nullptr) const;
  QLabel *title(const QString &text, QWidget *parent = nullptr) const;
  QLabel *body(const QString &text, QWidget *parent = nullptr) const;
  QPushButton *
  choiceButton(const QString &heading, const QString &detail, const QString &hint, bool extra = false) const;
  QPushButton *sideButton(const QString &text, Side side);
  QWidget *statusRow(const QString &dotStyle, const QString &name, const QString &state) const;

  // Flow
  void goTo(int step);
  void updateChrome();
  void applyStep(int step);
  void enterLink();
  void runChecks();
  void addCheckRow(const QString &text, const QString &sub, bool fixed);

  // Core process (runs only while the wizard is open)
  void startCore(bool asMain);
  void applyServerConfig();
  void stopCore();
  void prepareMachine();
  void onConnectedClients(const QStringList &clients);
  void onUnrecognisedClient(const QString &name);
  void onClientLimitReached(const QString &name);
  void onPeerFingerprint(const QString &fingerprint);
  void onLogLine(const QString &line);

  // Main computer: link list
  void refreshClientList();
  void addAnother();
  int nextFreeCell() const;
  QString lanAddress() const;
  QPixmap pairingQr() const;

  // Extra screen: connect + status
  void connectToMain();
  void resolveAndConnect(const QString &host, bool triedLocal);
  void setExtraStatus(ExtraState state, const QString &heading, const QString &detail);
  void showExtraError(const QString &heading, const QString &why);

  // Arrange
  void loadArrangeFor(int index);
  int cellFor(Side side) const;
  QString sideWord(Side side) const;

  // Chrome
  QStackedWidget *m_pages = nullptr;
  QProgressBar *m_progress = nullptr;
  QLabel *m_stepLabel = nullptr;
  QLabel *m_stepName = nullptr;
  QPushButton *m_back = nullptr;
  QPushButton *m_skip = nullptr;
  QPushButton *m_next = nullptr;

  // Pages
  QLineEdit *m_nameEdit = nullptr;
  QStackedWidget *m_linkStack = nullptr;

  QLabel *m_codeLabel = nullptr;
  QLabel *m_qrLabel = nullptr;
  QLabel *m_backupLabel = nullptr;
  QLabel *m_mainStatus = nullptr;
  QVBoxLayout *m_clientListLayout = nullptr;
  QPushButton *m_addAnotherBtn = nullptr;

  QLineEdit *m_extraCodeEdit = nullptr;
  QPushButton *m_connectBtn = nullptr;
  QWidget *m_extraStatusCard = nullptr;
  QLabel *m_extraStatusDot = nullptr;
  QLabel *m_extraStatusTitle = nullptr;
  QLabel *m_extraStatusDetail = nullptr;
  QPlainTextEdit *m_extraDetails = nullptr;
  QPushButton *m_copyDetailsBtn = nullptr;

  QLabel *m_arrangeTitle = nullptr;
  QLabel *m_arrangeIntro = nullptr;
  QLabel *m_mainTile = nullptr;
  QButtonGroup *m_sideGroup = nullptr;

  QLabel *m_doneHint = nullptr;
  QVBoxLayout *m_checksLayout = nullptr;
  QPushButton *m_mainChoice = nullptr;
  QPushButton *m_extraChoice = nullptr;

  // State
  Role m_role = Role::Main;
  QString m_name;
  QString m_mainName; // extra screen: the main computer it linked to

  std::unique_ptr<ServerConfig> m_serverConfig;
  std::unique_ptr<CoreProcess> m_core;
  QTimer *m_connectTimeout = nullptr;

  QStringList m_connected;  // main: connected right now
  QStringList m_everLinked; // main: connected at least once, in order
  QStringList m_pending;    // main: added to the layout, not connected yet
  QStringList m_trusted;    // fingerprints auto-trusted this session
  QStringList m_recentLog;
  bool m_waiting = true;
  bool m_fixedMode = false;
  bool m_fixedCert = false;

  ExtraState m_extraState = ExtraState::Idle;
  /// The main computer is adding us; keep retrying quietly until the timeout.
  bool m_awaitingAdmission = false;
  int m_arrangeIndex = 0;
  QMap<QString, Side> m_sideFor;
};

} // namespace deskflow::gui
