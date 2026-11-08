/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 - 2026 Chris Rizzitello <sithord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "Diagnostic.h"
#include "StyleUtils.h"

#include "dialogs/AboutDialog.h"
#include "dialogs/ClientConfigDialog.h"
#include "dialogs/FingerprintDialog.h"
#include "dialogs/HelpDialog.h"
#include "dialogs/ServerConfigDialog.h"
#include "dialogs/SettingsDialog.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/Messages.h"
#include "gui/TlsUtility.h"
#include "gui/core/CoreProcess.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/desk/AboutView.h"
#include "gui/desk/BrandIcons.h"
#include "gui/desk/ChecksView.h"
#include "gui/desk/ComputerLog.h"
#include "gui/desk/ComputerView.h"
#include "gui/desk/DeskView.h"
#include "gui/desk/NoticeCard.h"
#include "gui/desk/PlanView.h"
#include "gui/desk/SettingsView.h"
#include "gui/desk/ShortcutsView.h"
#include "gui/ipc/DaemonIpcClient.h"
#include "gui/widgets/LogDock.h"
#include "gui/wizard/SetupWizard.h"
#include "net/FingerprintDatabase.h"
#include "net/License.h"
#include "widgets/StatusBar.h"

#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QSysInfo>

#include <memory>

#if defined(Q_OS_MACOS)
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace deskflow::gui;

MainWindow::MainWindow()
    : ui{std::make_unique<Ui::MainWindow>()},
      m_coreProcess(m_serverConfig),
      m_trayIcon{new QSystemTrayIcon(this)},
      m_guiDupeChecker{new QLocalServer(this)},
      m_daemonIpcClient{new ipc::DaemonIpcClient(this)},
      m_logDock{new LogDock(this)},
      m_statusBar{new StatusBar(this)},
      m_menuFile{new QMenu(this)},
      m_menuEdit{new QMenu(this)},
      m_menuView{new QMenu(this)},
      m_menuHelp{new QMenu(this)},
      m_actionAbout{new QAction(this)},
      m_actionMinimize{new QAction(this)},
      m_actionQuit{new QAction(this)},
      m_actionTrayQuit{new QAction(this)},
      m_actionRestore{new QAction(this)},
      m_actionSettings{new QAction(this)},
      m_actionStartCore{new QAction(this)},
      m_actionRestartCore{new QAction(this)},
      m_actionStopCore{new QAction(this)},
      m_actionShowHelp{new QAction(this)},
      m_networkMonitor{new NetworkMonitor(this)}
{
  ui->setupUi(this);

  setWindowIcon(QIcon::fromTheme(kRevFqdnName));

  addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

  // Setup Actions
  m_actionAbout->setMenuRole(QAction::AboutRole);
  m_actionAbout->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout));

  m_actionMinimize->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
  m_actionRestore->setIcon(QIcon::fromTheme(QStringLiteral("window-restore-pip")));

  if (!deskflow::platform::isWindows()) {
    m_actionQuit->setShortcut(QKeySequence::Quit);
    m_actionTrayQuit->setShortcut(QKeySequence::Quit);
  }

  m_actionQuit->setIcon(QIcon::fromTheme("application-exit"));
  m_actionQuit->setMenuRole(QAction::QuitRole);

  m_actionTrayQuit->setIcon(QIcon::fromTheme("application-exit"));
  m_actionTrayQuit->setMenuRole(QAction::NoRole);

  m_actionSettings->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  m_actionSettings->setMenuRole(QAction::PreferencesRole);
  m_actionSettings->setShortcut(QKeySequence::Preferences);

  m_actionStartCore->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
  m_actionStartCore->setMenuRole(QAction::NoRole);

  m_actionRestartCore->setVisible(false);
  m_actionRestartCore->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
  m_actionRestartCore->setMenuRole(QAction::NoRole);

  m_actionStopCore->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop));
  m_actionStopCore->setMenuRole(QAction::NoRole);

  m_actionShowHelp->setIcon(QIcon::fromTheme(QStringLiteral("question")));
  m_actionShowHelp->setMenuRole(QAction::NoRole);
  m_actionShowHelp->setShortcut(QKeySequence::HelpContents);

  // Setup the Instance Checking
  // In case of a previous crash remove first
  QLocalServer::removeServer(m_guiSocketName);
  m_guiDupeChecker->listen(m_guiSocketName);

  createMenuBar();
  setupControls();
  updateText();
  connectSlots();
  setupTrayIcon();
  updateComputerName();
  setHelpFilePath();
  setupDeskView();

  qDebug().noquote() << "active settings path:" << Settings::settingsPath();

  // Force generation of a certificate on the host
  if (TlsUtility::isEnabled()) {
    if (Settings::value(Settings::Security::KeySize).toInt() < 2048) {
      // Nothing pops over anything: just do it and say so in the log / Checks activity.
      qInfo("TLS key is smaller than the minimum allowed size; generating a new 2048-bit key");
      Settings::setValue(Settings::Security::KeySize, 2048);
    }
    if (!TlsUtility::isCertValid()) {
      generateCertificate();
    } else {
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
    }
  }

  applyConfig();
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
  restoreWindow();

#ifdef Q_OS_MACOS
  // Route native quits (Cmd+Q / Apple menu Quit / Dock "Quit") to the usual close-to-tray decision instead.
  installQuitHandler([this] { return !maybeHideToTray(); });
#endif
}
MainWindow::~MainWindow()
{
  // Stop network monitoring
  if (m_networkMonitor) {
    m_networkMonitor->stopMonitoring();
  }

  m_guiDupeChecker->close();
  m_coreProcess.cleanup();
}

void MainWindow::restoreWindow()
{
  auto windowGeometry = Settings::value(Settings::Gui::WindowGeometry).toRect();
  const auto totalGeometry = QGuiApplication::primaryScreen()->availableGeometry();
  const bool classic = m_actionClassicView && m_actionClassicView->isChecked();
  if (!windowGeometry.isValid()) {
    if (classic)
      adjustSize();
    else
      resize(1080, 720);
    windowGeometry = geometry();
  } else {
    setGeometry(windowGeometry);
  }
  m_expandedSize = geometry().size();

  if (!totalGeometry.contains(windowGeometry)) {
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    move(screenGeometry.center() - rect().center());
  }

  if (!Settings::value(Settings::Gui::LogExpanded).toBool() && classic)
    setFixedSize(size());
}

void MainWindow::setupControls()
{
  secureSocket(false);

  ui->btnConfigureServer->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
  ui->btnConfigureClient->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

  if (Settings::value(Settings::Core::LastVersion).toString() != kVersion) {
    Settings::setValue(Settings::Core::LastVersion, kVersion);
  }

  if (!Settings::value(Settings::Gui::LogExpanded).toBool()) {
    m_logDock->hide();
  }

  ui->serverOptions->setVisible(false);
  ui->clientOptions->setVisible(false);

  const auto coreMode = Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>();
  ui->rbModeClient->setChecked(coreMode == Settings::CoreMode::Client);
  ui->rbModeServer->setChecked(coreMode == Settings::CoreMode::Server);

  ui->lineEditName->setValidator(new QRegularExpressionValidator(m_nameRegEx, this));
  ui->lineEditName->setVisible(false);
  ui->lineEditName->installEventFilter(this);

  if (deskflow::platform::isMac()) {
    ui->rbModeServer->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->rbModeClient->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->btnSaveServerConfig->setFixedWidth(ui->btnSaveServerConfig->height());
  } else {
    ui->btnSaveServerConfig->setIconSize(QSize(22, 22));
  }
  setStatusBar(m_statusBar);
}

//////////////////////////////////////////////////////////////////////////////
// Begin slots
//////////////////////////////////////////////////////////////////////////////

// remember: using queued connection allows the render loop to run before
// executing the slot. the default is to instantly call the slot when the
// signal is emitted from the thread that owns the receiver's object.
void MainWindow::connectSlots()
{
  connect(Settings::instance(), &Settings::serverSettingsChanged, this, &MainWindow::serverConfigSaving);
  connect(Settings::instance(), &Settings::settingsChanged, this, &MainWindow::settingsChanged);

  connect(&m_coreProcess, &CoreProcess::error, this, &MainWindow::coreProcessError);
  connect(&m_coreProcess, &CoreProcess::logLine, this, &MainWindow::handleLogLine);
  connect(
      &m_coreProcess, &CoreProcess::processStateChanged, this, &MainWindow::coreProcessStateChanged,
      Qt::QueuedConnection
  );
  connect(&m_coreProcess, &CoreProcess::connectionStateChanged, this, &MainWindow::coreConnectionStateChanged);
  connect(&m_coreProcess, &CoreProcess::secureSocket, this, &MainWindow::secureSocket);
  connect(
      &m_coreProcess, &CoreProcess::daemonIpcClientConnectionFailed, this, &MainWindow::daemonIpcClientConnectionFailed
  );
  connect(&m_coreProcess, &CoreProcess::securityLevelChanged, m_statusBar, &StatusBar::setSecurityLevel);

  connect(m_actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);
  connect(m_actionMinimize, &QAction::triggered, this, &MainWindow::hide);

  connect(m_actionQuit, &QAction::triggered, this, &MainWindow::close);
  connect(m_actionTrayQuit, &QAction::triggered, this, &MainWindow::close);
  connect(m_actionRestore, &QAction::triggered, this, &MainWindow::showAndActivate);
  connect(m_actionSettings, &QAction::triggered, this, &MainWindow::openSettings);
  connect(m_actionStartCore, &QAction::triggered, this, &MainWindow::startCore);
  connect(m_actionRestartCore, &QAction::triggered, this, &MainWindow::resetCore);
  connect(m_actionStopCore, &QAction::triggered, this, &MainWindow::stopCore);
  connect(m_actionShowHelp, &QAction::triggered, this, &MainWindow::showHelpViewer);

  // Mac os tray will only show a menu
  if (!deskflow::platform::isMac())
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);

  connect(&m_coreProcess, &CoreProcess::connectedClientsChanged, this, &MainWindow::serverClientsChanged);
  connect(&m_coreProcess, &CoreProcess::unrecognisedClient, this, &MainWindow::handleUnrecognisedClient);
  connect(&m_coreProcess, &CoreProcess::clientLimitReached, this, &MainWindow::handleClientLimitReached);
  connect(&m_coreProcess, &CoreProcess::connectionRefused, this, &MainWindow::handleConnectionRefused);
  connect(&m_coreProcess, &CoreProcess::retryIn, this, &MainWindow::updateTimeoutDelay);
  connect(&m_coreProcess, &CoreProcess::peerFingerprint, this, &MainWindow::handlePeerFingerprint);
  connect(&m_coreProcess, &CoreProcess::missingKeyboardLayouts, this, &MainWindow::handleMissingKeyboardLayouts);

  if (Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger, Qt::UniqueConnection);
  } else {
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);
  }

  connect(ui->btnRestartCore, &QPushButton::clicked, this, &MainWindow::resetCore);

  connect(ui->cbHostname->lineEdit(), &QLineEdit::returnPressed, ui->btnToggleCore, &QPushButton::click);
  connect(ui->cbHostname, &QComboBox::editTextChanged, this, &MainWindow::remoteHostChanged);

  connect(ui->btnSaveServerConfig, &QPushButton::clicked, this, &MainWindow::saveServerConfig);
  connect(ui->btnConfigureServer, &QPushButton::clicked, this, [this] { showConfigureServer(""); });
  connect(ui->btnConfigureClient, &QPushButton::clicked, this, [this] { showConfigureClient(); });
  connect(ui->lblComputerName, &QLabel::linkActivated, this, &MainWindow::openSettings);

  connect(ui->rbModeServer, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);
  connect(ui->rbModeClient, &QRadioButton::toggled, this, &MainWindow::coreModeToggled);

  connect(m_logDock->toggleViewAction(), &QAction::toggled, this, &MainWindow::toggleLogVisible);

  connect(m_statusBar, &StatusBar::requestShowMyFingerprints, this, &MainWindow::showMyFingerprint);
  connect(m_statusBar, &StatusBar::requestUpdateVersion, this, &MainWindow::openGetNewVersionUrl);
  connect(&m_versionChecker, &VersionChecker::updateFound, m_statusBar, &StatusBar::updateFound);

  connect(m_guiDupeChecker, &QLocalServer::newConnection, this, &MainWindow::showAndActivate);

  connect(ui->btnEditName, &QPushButton::clicked, this, &MainWindow::showHostNameEditor);

  connect(ui->lineEditName, &QLineEdit::editingFinished, this, &MainWindow::setHostName);

  connect(m_networkMonitor, &NetworkMonitor::ipAddressesChanged, this, &MainWindow::updateIpLabel);
}

void MainWindow::toggleLogVisible(bool visible)
{
  // When the main window is hidden e.g. close to tray / minimized), this also triggers the log visibility toggle,
  // but we don't want to hide the log in this case since we would need to un-hide it when the window is shown again.
  if (!isVisible() || isMinimized()) {
    qDebug() << "not toggling log, window not visible";
    return;
  }

  setFixedSize(16777215, 16777215);
  Settings::setValue(Settings::Gui::LogExpanded, visible);
  if (visible) {
    if (m_logDock->isFloating()) {
      adjustSize();
      setFixedSize(size());
    } else {
      QTimer::singleShot(15, this, [&] { resize(m_expandedSize); });
    }
  } else {
    if (!m_logDock->isFloating()) {
      m_expandedSize = geometry().size();
    }
    m_logDock->hide();
    if (!m_logDock->isFloating()) {
      adjustSize();
    }
    setFixedSize(size());
  }
  Settings::setValue(Settings::Gui::WindowGeometry, geometry());
}

void MainWindow::settingsChanged(const QString &key)
{
  if (key == Settings::Log::Level) {
    m_coreProcess.applyLogLevel();
    return;
  }

  if (key == Settings::Core::ComputerName)
    updateComputerName();

  if (key == Settings::Core::CoreMode || key == Settings::Client::RemoteHost ||
      key == Settings::Server::EnableClipboard || key == Settings::Security::TlsEnabled)
    refreshDeskView();

  if ((key == Settings::Security::Certificate) || (key == Settings::Security::KeySize) ||
      (key == Settings::Security::TlsEnabled) || (key == Settings::Security::CheckPeers)) {
    if (TlsUtility::isEnabled()) {
      if (!TlsUtility::isCertValid()) {
        qWarning() << tr("invalid certificate, generating a new one");
        TlsUtility::generateCertificate();
      }
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
      updateFingerprintButton();
    }
    updateSecurityIcon(m_statusBar->securityIconVisible());
    return;
  }
}

void MainWindow::serverConfigSaving()
{
  m_serverConfig.commit();
  refreshDeskView();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
  if (reason != QSystemTrayIcon::Trigger)
    return;
  isVisible() ? hide() : showAndActivate();
}

void MainWindow::coreProcessError(CoreProcess::Error error)
{
  using deskflow::gui::NoticeCard;
  if (error == CoreProcess::Error::AddressMissing) {
    if (auto *card = deskNotice()) {
      card->setIcon(deskflow::gui::brand::Icon::Monitor, NoticeCard::Tone::Danger)
          ->setTitle(tr("Which computer is the main one?"))
          ->setBody(
              tr("This computer is set up as an extra screen but doesn't know the main computer's name yet. "
                 "Run setup again from Add a computer, or pick the main computer in Settings.")
          )
          ->setButtons({tr("OK")});
      connect(card, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
      return;
    }
    QMessageBox::warning(
        this, tr("Address missing"), tr("Please enter the hostname or IP address of the other computer.")
    );
  } else if (error == CoreProcess::Error::StartFailed) {
    show();
    auto message = tr("The Core executable could not be started.\n"
                      "Please check if you have sufficient permissions to run %1.")
                       .arg(kCoreBinName);

    if (Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Server) {
      const auto mode =
          Settings::value(Settings::Server::ExternalConfigFile).toBool() ? tr("read") : tr("read and write");
      message.append(tr("\nAdditionally, check you are able to %1 the server config file: %2")
                         .arg(mode, Settings::serverConfigFile()));
    }
    if (auto *card = deskNotice()) {
      card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Danger)
          ->setTitle(tr("Lightning Desk couldn't start sharing"))
          ->setBody(message)
          ->setButtons({tr("OK"), tr("Try again")});
      connect(card, &NoticeCard::answered, this, [this](int button, int) {
        m_deskView->dismissNotice();
        if (button == 1)
          startCore();
      });
      return;
    }
    QMessageBox::warning(this, kAppName, message);
  }
}

void MainWindow::startCore()
{
  // Save current IP state when server starts
  if (m_coreProcess.mode() == CoreMode::Server && Settings::value(Settings::Core::Interface).toString().isEmpty()) {
    m_serverStartIPs = NetworkMonitor::validAddresses();
    m_serverStartSuggestedIP = m_serverStartIPs.isEmpty() ? "" : m_serverStartIPs.first();
  }

  m_actionStartCore->setVisible(false);
  m_actionRestartCore->setVisible(true);
  m_coreProcess.start();
}

void MainWindow::stopCore()
{
  qDebug() << "stopping core process";
  m_coreProcess.stop();
  m_actionStartCore->setVisible(true);
  m_actionRestartCore->setVisible(false);
}

void MainWindow::clearSettings()
{
  qDebug() << "clearing settings";

  m_networkMonitor->stopMonitoring();

  disconnect(&m_coreProcess, nullptr, this, nullptr);
  disconnect(&m_versionChecker, nullptr, this, nullptr);
  disconnect(m_guiDupeChecker, nullptr, this, nullptr);
  disconnect(m_trayIcon, nullptr, this, nullptr);
  disconnect(m_logDock->toggleViewAction(), nullptr, this, nullptr);

  m_coreProcess.stop();
  m_coreProcess.clearSettings();

  m_saveOnExit = false;
  diagnostic::clearSettings(true);
}

bool MainWindow::saveServerConfig()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save server configuration as..."));

  if (!fileName.isEmpty() && !m_serverConfig.save(fileName)) {
    QMessageBox::warning(this, tr("Save failed"), tr("Could not save server configuration to file."));
    return true;
  }

  return false;
}

void MainWindow::openAboutDialog()
{
  AboutDialog about(this);
  about.exec();
}

void MainWindow::openGetNewVersionUrl() const
{
  QDesktopServices::openUrl(QUrl(kUrlDownload));
}

void MainWindow::openSettings()
{
  auto dialog = SettingsDialog(this, m_serverConfig);

  connect(&dialog, &SettingsDialog::requestRemoveAllSettings, this, &MainWindow::clearSettings, Qt::UniqueConnection);
  if (dialog.exec() == QDialog::Accepted) {
    Settings::save();
    disconnect(&dialog, &SettingsDialog::requestRemoveAllSettings, nullptr, nullptr);

    applyConfig();

    if (m_coreProcess.isStarted()) {
      m_coreProcess.restart();
    }
  }
}

void MainWindow::resetCore()
{
  m_coreProcess.restart();
}

void MainWindow::showMyFingerprint()
{
  FingerprintDialog fingerprintDialog(this, m_fingerprint);
  fingerprintDialog.exec();
}

void MainWindow::coreModeToggled(bool checked)
{
  // this method is called when rbClient or rbServer toggles
  // with both being in the same group one must be turned on if the other is turned off
  // only react to toggle on to avoid calling everything twice when the user switches modes
  if (!checked)
    return;

  Settings::CoreMode mode = Settings::CoreMode::None;

  if (ui->rbModeServer->isChecked())
    mode = Settings::CoreMode::Server;
  if (ui->rbModeClient->isChecked())
    mode = Settings::CoreMode::Client;

  qDebug() << QStringLiteral("change mode to: %1").arg(QVariant::fromValue(mode).toString());

  if (m_coreProcess.isStarted() && m_coreProcess.mode() != mode)
    m_coreProcess.stop();
  m_coreProcess.setMode(mode);

  Settings::setValue(Settings::Core::CoreMode, mode);
  Settings::save();

  updateModeControls();
}

void MainWindow::updateModeControls()
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;

  ui->serverOptions->setVisible(isServer);
  ui->clientOptions->setVisible(isClient);
  ui->lblNoMode->setVisible(!isServer && !isClient);
  toggleCanRunCore(canRunCore());

  ui->lblIpAddresses->setVisible(
      (isClient && !Settings::value(Settings::Core::Interface).toString().isEmpty()) || isServer
  );

  if (ui->lblIpAddresses->isVisible())
    updateNetworkInfo();

  if (isServer) {
    m_networkMonitor->startMonitoring();
  } else {
    m_networkMonitor->stopMonitoring();
  }

  if (isServer || isClient)
    updateModeControlLabels();
}

void MainWindow::updateModeControlLabels()
{
  const bool isServer = m_coreProcess.mode() == CoreMode::Server;
  const bool isStarted = m_coreProcess.isStarted();

  QString startText;
  QString stopText;
  QIcon startIcon;
  QIcon stopIcon;

  if (isServer) {
    startText = tr("Start");
    stopText = tr("Stop");
    startIcon = QIcon::fromTheme(QStringLiteral("system-run"));
    stopIcon = QIcon::fromTheme(QIcon::ThemeIcon::ProcessStop);
  } else {
    startText = tr("Connect");
    stopText = tr("Disconnect");
    startIcon = QIcon::fromTheme(QStringLiteral("network-connect"));
    stopIcon = QIcon::fromTheme(QStringLiteral("network-disconnect"));
  }

  m_actionStartCore->setText(startText);
  m_actionStartCore->setIcon(startIcon);
  m_actionStopCore->setText(stopText);
  m_actionStopCore->setIcon(stopIcon);

  if (isStarted) {
    ui->btnToggleCore->setText(stopText);
    ui->btnToggleCore->setIcon(stopIcon);
  } else {
    ui->btnToggleCore->setText(startText);
    ui->btnToggleCore->setIcon(startIcon);
  }
}

void MainWindow::updateSecurityIcon(bool visible)
{
  m_statusBar->setSecurityIconVisible(visible);
  if (!visible)
    return;
  m_statusBar->setSecurityIcon(TlsUtility::isEnabled());
}

void MainWindow::updateNetworkInfo()
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  if (mode == CoreMode::Server)
    updateIpLabel(NetworkMonitor::validAddresses());
  else
    updateIpLabel({Settings::value(Settings::Core::Interface).toString()});
}

void MainWindow::serverConnectionConfigureClient(const QString &clientName)
{
  m_serverConfigDialogVisible = true;
  ServerConfigDialog dialog(this, m_serverConfig);
  if (dialog.addClient(clientName) && dialog.exec() == QDialog::Accepted) {
    m_coreProcess.restart();
  }
  m_serverConfigDialogVisible = false;
}

//////////////////////////////////////////////////////////////////////////////
// End slots
//////////////////////////////////////////////////////////////////////////////

void MainWindow::open()
{
  if (!Settings::value(Settings::Gui::Autohide).toBool())
    showAndActivate();
  else if (deskflow::platform::isMac())
    // macOS to call hide after this function ends
    QTimer::singleShot(1, this, &MainWindow::hide);
  else
    hide();

  // if a critical error was shown just before the main window (i.e. on app
  // load), it will be hidden behind the main window. therefore we need to raise
  // it up in front of the main window.
  // HACK: because the `onShown` event happens just as the window is shown, the
  // message box has a chance of being raised under the main window. to solve
  // this we delay the error dialog raise by a split second. this seems a bit
  // hacky and fragile, so maybe there's a better approach.
  const auto kCriticalDialogDelay = 100;
  QTimer::singleShot(kCriticalDialogDelay, this, &messages::raiseCriticalDialog);

  if (!Settings::value(Settings::Gui::AutoUpdateCheck).isValid()) {
    if (auto *card = deskNotice()) {
      using deskflow::gui::NoticeCard;
      card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Neutral)
          ->setTitle(tr("Tell you when an update is ready?"))
          ->setBody(
              tr("Lightning Desk checks schomp.ai now and then. Nothing installs without you; you can change "
                 "this in Settings any time.")
          )
          ->setButtons({tr("No thanks"), tr("Yes, tell me")});
      connect(card, &NoticeCard::answered, this, [this](int button, int) {
        m_deskView->dismissNotice();
        Settings::setValue(Settings::Gui::AutoUpdateCheck, button == 1);
        if (button == 1)
          m_versionChecker.checkLatest();
      });
    } else {
      Settings::setValue(Settings::Gui::AutoUpdateCheck, messages::showUpdateCheckOption(this));
    }
  }

  if (Settings::value(Settings::Gui::AutoUpdateCheck).toBool()) {
    m_versionChecker.checkLatest();
  } else {
    qDebug() << "skipping check for new version, disabled";
  }

  if (Settings::value(Settings::Gui::AutoStartCore).toBool()) {
    if (ui->rbModeClient->isChecked() && ui->cbHostname->currentText().isEmpty())
      return;
    startCore();
  }
}

//////////////////////////////////////////////////////////////////////////////
// Lightning Desk: branded main screen
//////////////////////////////////////////////////////////////////////////////

void MainWindow::setupDeskView()
{
  m_computerLog = new deskflow::gui::ComputerLog(this);
  connect(m_computerLog, &deskflow::gui::ComputerLog::changed, this, [this](const QString &name) {
    if (m_deskView && m_deskView->onComputerPage() && m_deskView->computer()->computerName() == name)
      m_deskView->computer()->setDetails(m_computerLog->entry(name));
  });
  using deskflow::gui::DeskView;

  m_deskView = new DeskView(this);
  m_classicCentral = takeCentralWidget();
  m_centralStack = new QStackedWidget(this);
  m_centralStack->addWidget(m_deskView);
  if (m_classicCentral)
    m_centralStack->addWidget(m_classicCentral);
  setCentralWidget(m_centralStack);

  // The classic (inherited) window stays one toggle away while the new screens land.
  m_actionClassicView = new QAction(tr("Classic view"), this);
  m_actionClassicView->setCheckable(true);
  m_actionClassicView->setChecked(Settings::value(Settings::Gui::ClassicView).toBool());
  m_menuView->addAction(m_actionClassicView);
  connect(m_actionClassicView, &QAction::toggled, this, &MainWindow::toggleClassicView);

  connect(m_deskView, &DeskView::sharingToggled, this, [this](bool on) {
    if (on)
      startCore();
    else
      stopCore();
  });
  connect(m_deskView, &DeskView::addComputerRequested, this, &MainWindow::addComputerFromDesk);
  connect(m_deskView, &DeskView::changeMainRequested, this, &MainWindow::changeMainFromDesk);
  connect(m_deskView, &DeskView::moveRequested, this, [this](const QString &name, int cell) {
    if (!m_serverConfig.moveClient(name, cell))
      return;
    m_serverConfig.commit();
    refreshDeskView();
    if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
      m_coreProcess.restart();
    if (m_deskView->onComputerPage())
      openComputerPage(name); // re-show with the new spot highlighted
  });
  connect(m_deskView, &DeskView::computerClicked, this, &MainWindow::openComputerPage);
  connect(
      m_deskView->computer(), &deskflow::gui::ComputerView::computerChanged, this,
      [this](const Computer &computer) {
        if (!m_serverConfig.updateClient(computer))
          return;
        m_serverConfig.commit();
        if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
          m_coreProcess.restart();
      }
  );
  connect(m_deskView, &DeskView::forgetRequested, this, [this](const QString &name) {
    m_serverConfig.removeClient(name);
    m_serverConfig.commit();
    m_ignoredClients.remove(name);
    refreshDeskView();
    if (m_coreProcess.isStarted())
      m_coreProcess.restart(); // drops the live connection; it has to be allowed again
  });
  connect(m_deskView, &DeskView::settingsRequested, this, &MainWindow::refreshDeskView);
  auto *settingsPage = m_deskView->settings();
  using deskflow::gui::SettingsView;
  connect(settingsPage, &SettingsView::coreSettingChanged, this, [this] {
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });
  connect(settingsPage, &SettingsView::serverSettingChanged, this, [this] {
    m_serverConfig.commit();
    if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
      m_coreProcess.restart();
  });
  connect(settingsPage, &SettingsView::openLogsRequested, this, [] {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::settingsPath()));
  });
  connect(settingsPage, &SettingsView::planClicked, this, [this] {
    refreshDeskView();
    m_deskView->showPlanPage();
  });
  using deskflow::gui::PlanView;
  connect(m_deskView->plan(), &PlanView::getPlusRequested, this, [] {
    QDesktopServices::openUrl(QUrl(QStringLiteral("%1?%2").arg(kUrlPlus, kUrlSourceQuery)));
  });
  connect(m_deskView->plan(), &PlanView::licenseSaved, this, [this] {
    refreshDeskView();
    using deskflow::gui::NoticeCard;
    const auto &lic = deskflow::License::current(true);
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Neutral)
        ->setTitle(tr("Plus is on"))
        ->setBody(tr("This desk now links this computer and up to %n more. Licensed to %1.", "",
                     lic.maxExtraComputers())
                      .arg(lic.email()))
        ->setButtons({tr("Great")});
    connect(card, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
    if (m_coreProcess.isStarted())
      m_coreProcess.restart(); // the server re-reads the license on start
  });
  connect(settingsPage, &SettingsView::newKeyRequested, this, [this] {
    using deskflow::gui::NoticeCard;
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Lock, NoticeCard::Tone::Neutral)
        ->setTitle(tr("Make a new security key?"))
        ->setBody(tr("Every linked computer will ask you to allow this one again the next time it connects."))
        ->setButtons({tr("Keep the current key"), tr("Make a new key")});
    connect(card, &NoticeCard::answered, this, [this](int button, int) {
      m_deskView->dismissNotice();
      if (button != 1)
        return;
      const bool wasRunning = m_coreProcess.isStarted();
      if (wasRunning)
        stopCore();
      TlsUtility::generateCertificate();
      m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
      updateFingerprintButton();
      if (wasRunning)
        startCore();
      refreshDeskView();
    });
  });
  connect(settingsPage, &SettingsView::resetRequested, this, [this] {
    const bool wasRunning = m_coreProcess.isStarted();
    if (wasRunning)
      stopCore();
    Settings::restoreDefaultSettings();
    Settings::setValue(Settings::Gui::WizardCompleted, true);
    m_serverConfig.reload();
    m_serverConfig.commit();
    refreshDeskView();
    m_deskView->showDeskPage();
  });
  connect(m_deskView, &DeskView::shortcutsRequested, this, &MainWindow::refreshDeskView);
  connect(m_deskView->shortcuts(), &deskflow::gui::ShortcutsView::addRequested, this, [this](const Hotkey &hotkey) {
    m_serverConfig.addHotkey(hotkey);
    m_serverConfig.commit();
    refreshDeskView();
    if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
      m_coreProcess.restart();
  });
  connect(m_deskView->shortcuts(), &deskflow::gui::ShortcutsView::removeRequested, this, [this](int index) {
    m_serverConfig.removeHotkeyAt(index);
    m_serverConfig.commit();
    refreshDeskView();
    if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
      m_coreProcess.restart();
  });
  connect(m_deskView, &DeskView::helpRequested, this, [this] {
    m_deskView->about()->setSecurityCode(QString::fromLatin1(m_fingerprint.data.toHex()));
  });
  auto *aboutPage = m_deskView->about();
  using deskflow::gui::AboutView;
  connect(aboutPage, &AboutView::checkForUpdatesRequested, this, [this] {
    m_deskView->about()->setChecking();
    m_versionChecker.checkLatest();
  });
  connect(&m_versionChecker, &VersionChecker::checkFinished, aboutPage, &AboutView::setUpdateState);
  connect(aboutPage, &AboutView::getUpdateRequested, this, &MainWindow::openGetNewVersionUrl);
  connect(aboutPage, &AboutView::openLogsRequested, this, [] {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::settingsPath()));
  });
  connect(aboutPage, &AboutView::copyDiagnosticsRequested, this, [this] {
    QStringList lines{
        QStringLiteral("Lightning Desk %1 (%2)").arg(kVersion, kVersionGitSha), QSysInfo::prettyProductName(),
        QStringLiteral("mode: %1").arg(Settings::value(Settings::Core::CoreMode).toString()),
        QStringLiteral("computer: %1").arg(Settings::value(Settings::Core::ComputerName).toString()),
        QStringLiteral("settings: %1").arg(Settings::settingsPath())
    };
    for (const auto &computer : std::as_const(m_serverConfig).computers())
      if (!computer.isNull())
        lines << QStringLiteral("desk: %1").arg(computer.name());
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    using deskflow::gui::NoticeCard;
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Clipboard, NoticeCard::Tone::Neutral)
        ->setTitle(tr("Diagnostics copied"))
        ->setBody(tr("Paste them into your email to %1.").arg(kUrlSupportMail))
        ->setButtons({tr("OK")});
    connect(card, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
  });
  connect(aboutPage, &AboutView::checksRequested, this, [this] {
    refreshChecks();
    m_deskView->showChecksPage();
  });
  auto *checksPage = m_deskView->checks();
  using deskflow::gui::ChecksView;
  connect(checksPage, &ChecksView::rerunRequested, this, &MainWindow::refreshChecks);
  connect(checksPage, &ChecksView::copyReportRequested, this, [this] {
    refreshChecks();
    QGuiApplication::clipboard()->setText(
        m_deskView->checks()->report() + QStringLiteral("\n\n") + m_computerLog->report()
    );
    using deskflow::gui::NoticeCard;
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Clipboard, NoticeCard::Tone::Neutral)
        ->setTitle(tr("Report copied"))
        ->setBody(tr("Paste it into your email to %1.").arg(kUrlSupportMail))
        ->setButtons({tr("OK")});
    connect(card, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
  });
  connect(checksPage, &ChecksView::actionRequested, this, [this](const QString &id) {
    if (id == QLatin1String("start")) {
      m_actionStartCore->trigger();
      m_deskView->showChecksPage();
    } else if (id == QLatin1String("settings")) {
      m_deskView->showSettingsPage();
    } else if (id == QLatin1String("newkey")) {
      if (generateCertificate() && m_coreProcess.isStarted())
        m_coreProcess.restart();
      refreshChecks();
    } else if (id.startsWith(QLatin1String("howto:"))) {
      const auto name = id.mid(6);
      using deskflow::gui::NoticeCard;
      auto *card = m_deskView->notice();
      card->setIcon(deskflow::gui::brand::Icon::Monitor, NoticeCard::Tone::Accent)
          ->setTitle(tr("Getting %1 back").arg(name))
          ->setBody(
              tr("1. Wake it up and make sure it's on the same Wi‑Fi or network.\n"
                 "2. Open Lightning Desk on it. It should say it's linked to %1.\n"
                 "3. If it asks to trust this computer, check the security code matches the one on the Help page.\n"
                 "4. Still nothing? Forget it from Your desk and add it again.")
                  .arg(Settings::value(Settings::Core::ComputerName).toString())
          )
          ->setButtons({tr("OK")});
      connect(card, &NoticeCard::answered, this, [this](int, int) {
        m_deskView->dismissNotice();
        m_deskView->showChecksPage();
      });
    }
  });
  connect(m_deskView, &DeskView::clipboardToggled, this, [this](bool) {
    if (m_coreProcess.isStarted())
      m_coreProcess.restart();
  });

  // Critical errors become an inline notice instead of a message box.
  messages::setCriticalSink([this](const QString &message, const QString &fileLine) {
    auto *card = deskNotice();
    if (!card) {
      messages::setCriticalSink(nullptr); // classic view: fall back to the box
      return;
    }
    using deskflow::gui::NoticeCard;
    card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Danger)
        ->setTitle(tr("Something went wrong"))
        ->setBody(tr("%1\n\nIf it keeps happening, copy a report from Help › Something not working? and send it to %2.")
                      .arg(message, kUrlSupportMail))
        ->setButtons({tr("OK")});
    Q_UNUSED(fileLine); // it's in the log and the Checks report
    connect(card, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
  });

  toggleClassicView(m_actionClassicView->isChecked());
  refreshDeskView();
}

deskflow::gui::NoticeCard *MainWindow::deskNotice()
{
  if (!m_deskView || !m_actionClassicView || m_actionClassicView->isChecked())
    return nullptr;
  showAndActivate();
  return m_deskView->notice();
}

void MainWindow::toggleClassicView(bool classic)
{
  Settings::setValue(Settings::Gui::ClassicView, classic);
  if (classic && m_classicCentral)
    m_centralStack->setCurrentWidget(m_classicCentral);
  else
    m_centralStack->setCurrentWidget(m_deskView);
  if (menuBar())
    menuBar()->setVisible(classic);
  m_statusBar->setVisible(classic);
  if (classic) {
    setMinimumSize(0, 0);
    return;
  }
  // The Desk screen is a real window: resizable, with room for the grid.
  setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  setMinimumSize(960, 620);
  if (width() < 1080 || height() < 700)
    resize(1080, 720);
  // The log is a classic-view tool; on the Desk screen it only steals the grid's room.
  m_logDock->hide();
}

void MainWindow::refreshDeskView()
{
  if (!m_deskView)
    return;
  const auto thisName = Settings::value(Settings::Core::ComputerName).toString();
  if (Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>() == Settings::CoreMode::Client) {
    m_deskView->setLinkedTo(thisName, Settings::value(Settings::Client::RemoteHost).toString());
  } else {
    m_deskView->setDesk(thisName, m_serverConfig);
  }
  m_deskView->setSharing(m_coreProcess.isStarted());
  m_deskView->setConnectionState(m_coreProcess.connectionState());

  int trusted = 0;
  if (TlsUtility::isEnabled()) {
    FingerprintDatabase db;
    db.read(trustedFingerprintDatabase());
    trusted = static_cast<int>(db.fingerprints().size());
  }
  m_deskView->setTrustedCount(trusted);
  m_deskView->refreshConveniences();

  QStringList names{thisName};
  for (const auto &computer : std::as_const(m_serverConfig).computers())
    if (!computer.isNull() && computer.name() != thisName)
      names << computer.name();
  m_deskView->shortcuts()->setShortcuts(std::as_const(m_serverConfig).hotkeys(), names, thisName);

  // Settings page: plan row + trusted computers
  const auto &license = deskflow::License::current(true);
  const int extras = license.maxExtraComputers();
  if (license.isLicensed()) {
    const auto tier = license.tier();
    const auto exp = license.expires();
    m_deskView->settings()->setPlan(
        tr("Lightning Desk %1").arg(tier.left(1).toUpper() + tier.mid(1)),
        exp.isValid() ? tr("Until %1 · this computer + up to %n more", "", extras).arg(exp.date().toString(Qt::ISODate))
                      : tr("This computer + up to %n more", "", extras),
        tier.compare(QStringLiteral("trial"), Qt::CaseInsensitive) == 0 ? tr("Trial") : tr("Active")
    );
  } else {
    m_deskView->settings()->setPlan(
        tr("Lightning Desk Free"), tr("This computer + %n more · Plus links up to five", "", extras), QString()
    );
  }
  m_deskView->settings()->setTrusted(trusted, names.mid(1));
  m_deskView->settings()->reload();
  m_deskView->plan()->reload(names.size());
  refreshChecks();
  refreshTrayMenu();
}

void MainWindow::refreshChecks()
{
  if (!m_deskView)
    return;
  using deskflow::gui::ChecksView;
  using Status = ChecksView::Status;
  QList<ChecksView::Check> checks;
  const bool server = m_coreProcess.mode() == CoreMode::Server;
  const bool running = m_coreProcess.isStarted();
  const auto state = m_coreProcess.connectionState();
  const auto thisName = Settings::value(Settings::Core::ComputerName).toString();
  const auto mainName = Settings::value(Settings::Client::RemoteHost).toString();

  // 1. Sharing / link
  if (server) {
    if (running)
      checks << ChecksView::Check{tr("Sharing"), tr("On from %1").arg(thisName), Status::Ok};
    else
      checks << ChecksView::Check{tr("Sharing is off"), tr("Other computers can't join until it's on."),
                                  Status::Warn,         {},
                                  tr("Turn on"),        QStringLiteral("start")};
  } else {
    if (state == ConnectionState::Connected)
      checks << ChecksView::Check{
          tr("Linked to %1").arg(mainName), tr("Keyboard and mouse come from there"), Status::Ok
      };
    else if (running)
      checks << ChecksView::Check{
          tr("Looking for %1").arg(mainName),
          tr("Make sure Lightning Desk is open and sharing is on over there, and both computers are on the same "
             "network."),
          Status::Warn
      };
    else
      checks << ChecksView::Check{tr("Not linked"), tr("This computer isn't looking for %1 right now.").arg(mainName),
                                  Status::Warn,     {},
                                  tr("Link now"),   QStringLiteral("start")};
  }

  // 2. Network
  const auto addrs = NetworkMonitor::validAddresses();
  if (addrs.isEmpty())
    checks << ChecksView::Check{
        tr("Not on a network"), tr("Connect to Wi‑Fi or plug in a cable, then run the checks again."), Status::Bad
    };
  else
    checks << ChecksView::Check{
        tr("Network"), tr("This computer is reachable at %1").arg(addrs.join(QStringLiteral(", "))), Status::Ok
    };

  // 3. Port (server only)
  if (server) {
    const int port = Settings::value(Settings::Core::Port).toInt();
    const auto iface = Settings::value(Settings::Core::Interface).toString();
    checks << ChecksView::Check{
        tr("Connection port"),
        iface.isEmpty() ? tr("Other computers connect on port %1. If a firewall asks, allow Lightning Desk.").arg(port)
                        : tr("Other computers connect to %1 on port %2.").arg(iface).arg(port),
        Status::Info
    };
  }

  // 4. Encryption
  if (TlsUtility::isEnabled()) {
    const auto cert = Settings::value(Settings::Security::Certificate).toString();
    if (QFile::exists(cert))
      checks << ChecksView::Check{tr("Encryption"), tr("Connections between your computers are encrypted"), Status::Ok};
    else
      checks << ChecksView::Check{
          tr("Security key missing"),
          tr("We can make a new one. Other computers will ask you to trust it again."),
          Status::Bad,
          {},
          tr("Make a new key"),
          QStringLiteral("newkey")
      };
  } else {
    checks << ChecksView::Check{tr("Encryption is off"),
                                tr("Anyone on your network could read what you type."),
                                Status::Warn,
                                {},
                                tr("Turn on"),
                                QStringLiteral("settings")};
  }

  // 5. Each computer on the desk (server only)
  if (server) {
    for (const auto &computer : std::as_const(m_serverConfig).computers()) {
      if (computer.isNull() || computer.name() == thisName)
        continue;
      const auto name = computer.name();
      if (m_connectedClients.contains(name))
        checks << ChecksView::Check{name, tr("Linked"), Status::Ok};
      else if (running)
        checks << ChecksView::Check{
            tr("%1 isn't answering").arg(name),
            tr("It may be asleep, or Lightning Desk may have been closed on it."),
            Status::Warn,
            {},
            tr("How to fix"),
            QStringLiteral("howto:%1").arg(name)
        };
      else
        checks << ChecksView::Check{name, tr("Waiting for sharing to turn on"), Status::Info};
    }
  }

  m_deskView->checks()->setChecks(checks);
}

void MainWindow::openComputerPage(const QString &name)
{
  if (!m_deskView)
    return;
  const int idx = m_serverConfig.indexOf(name);
  if (idx < 0)
    return;
  const auto &computer = std::as_const(m_serverConfig).computers().at(idx);
  if (computer.isServer() || computer.name() == Settings::value(Settings::Core::ComputerName).toString())
    return;
  const bool linked = m_connectedClients.contains(name);
  const QString status = linked ? tr("Linked") : m_coreProcess.isStarted() ? tr("Waiting") : tr("Off");
  m_deskView->showComputerPage(computer, status, linked);
  m_deskView->computer()->setDetails(m_computerLog->entry(name));
}

void MainWindow::changeMainFromDesk()
{
  if (!m_deskView)
    return;
  const auto mainName = Settings::value(Settings::Client::RemoteHost).toString();
  if (mainName.isEmpty()) {
    linkAsExtraFromDesk();
    return;
  }
  using deskflow::gui::NoticeCard;
  auto *card = m_deskView->notice();
  card->setIcon(deskflow::gui::brand::Icon::Monitor, NoticeCard::Tone::Accent)
      ->setTitle(tr("Linked to %1").arg(mainName))
      ->setBody(
          tr("Link this computer to a different main computer, or unlink it so it's on its own again. "
             "Its mouse and keyboard work normally either way.")
      )
      ->setButtons({tr("Keep"), tr("Unlink"), tr("Link to a different computer")});
  connect(card, &NoticeCard::answered, this, [this, mainName](int button, int) {
    m_deskView->dismissNotice();
    if (button == 1) {
      if (m_coreProcess.isStarted())
        stopCore();
      Settings::setValue(Settings::Client::RemoteHost, QString());
      refreshDeskView();
      auto *done = m_deskView->notice();
      using deskflow::gui::NoticeCard;
      done->setIcon(deskflow::gui::brand::Icon::Monitor, NoticeCard::Tone::Neutral)
          ->setTitle(tr("Unlinked from %1").arg(mainName))
          ->setBody(
              tr("This computer is on its own now. Press “Link to a computer” whenever you want it back on a desk.")
          )
          ->setButtons({tr("OK")});
      connect(done, &NoticeCard::answered, this, [this](int, int) { m_deskView->dismissNotice(); });
    } else if (button == 2) {
      linkAsExtraFromDesk();
    }
  });
}

void MainWindow::linkAsExtraFromDesk()
{
  if (m_coreProcess.isStarted())
    stopCore();
  auto *wizard = new deskflow::gui::SetupWizard(this);
  wizard->setAttribute(Qt::WA_DeleteOnClose);
  const bool inline_ = m_deskView && m_actionClassicView && !m_actionClassicView->isChecked();
  if (inline_) {
    wizard->embedInto(m_deskView);
    m_deskView->showPage(wizard);
  }
  connect(wizard, &QDialog::finished, this, [this, wizard, inline_](int result) {
    if (inline_) {
      m_deskView->removePage(wizard);
      m_deskView->showDeskPage();
    }
    refreshDeskView();
    // The wizard's own core stops when it closes; run ours against whatever main is now set.
    if (result == QDialog::Accepted && !Settings::value(Settings::Client::RemoteHost).toString().isEmpty())
      startCore();
  });
  wizard->openAtLinkAsExtra();
}

void MainWindow::addComputerFromDesk(int cell)
{
  const bool wasRunning = m_coreProcess.isStarted();
  if (wasRunning)
    stopCore();

  QStringList before;
  for (const auto &computer : std::as_const(m_serverConfig).computers())
    if (!computer.isNull())
      before << computer.name();

  auto *wizard = new deskflow::gui::SetupWizard(this);
  wizard->setAttribute(Qt::WA_DeleteOnClose);
  const bool inline_ = m_deskView && m_actionClassicView && !m_actionClassicView->isChecked();
  if (inline_) {
    // Nothing pops over anything: the link flow runs as a page inside the Desk screen.
    wizard->embedInto(m_deskView);
    m_deskView->showPage(wizard);
  }
  connect(wizard, &QDialog::finished, this, [this, wasRunning, before, cell, wizard, inline_](int) {
    if (inline_) {
      m_deskView->removePage(wizard);
      m_deskView->showDeskPage();
    }
    m_serverConfig.reload();
    // The wizard drops a new computer into the next free cell; move it to the one that was clicked.
    if (cell >= 0) {
      for (const auto &computer : std::as_const(m_serverConfig).computers()) {
        if (!computer.isNull() && !before.contains(computer.name())) {
          if (m_serverConfig.moveClient(computer.name(), cell))
            m_serverConfig.commit();
          break;
        }
      }
    }
    refreshDeskView();
    if (wasRunning || Settings::value(Settings::Gui::AutoStartCore).toBool())
      startCore();
  });
  wizard->openAtLink();
}

void MainWindow::createMenuBar()
{
  m_menuFile->addAction(m_actionStartCore);
  m_menuFile->addAction(m_actionRestartCore);
  m_menuFile->addAction(m_actionStopCore);
  m_menuFile->addSeparator();
  m_menuFile->addAction(m_actionQuit);

  m_menuEdit->addAction(m_actionSettings);

  m_menuView->addAction(m_logDock->toggleViewAction());

  m_menuHelp->addAction(m_actionAbout);
  m_menuHelp->addAction(m_actionShowHelp);

  auto menuBar = new QMenuBar(this);
  menuBar->addMenu(m_menuFile);
  menuBar->addMenu(m_menuEdit);
  menuBar->addMenu(m_menuView);
  menuBar->addMenu(m_menuHelp);

  setMenuBar(menuBar);
}

void MainWindow::setupTrayIcon()
{
  // The tray menu is OS-native (the one popup we can't avoid), so it stays short and reads like
  // the app: what's happening, who's linked, pause/resume, add, open, quit.
  m_trayMenu = new QMenu(this);
  m_actionTrayToggle = new QAction(this);
  connect(m_actionTrayToggle, &QAction::triggered, this, [this] {
    if (m_coreProcess.isStarted())
      m_actionStopCore->trigger();
    else
      m_actionStartCore->trigger();
  });
  m_actionTrayAdd = new QAction(tr("Add a computer…"), this);
  connect(m_actionTrayAdd, &QAction::triggered, this, [this] {
    showAndActivate();
    addComputerFromDesk(-1);
  });
  m_trayIcon->setContextMenu(m_trayMenu);
  refreshTrayMenu();

  setTrayIcon();
  m_trayIcon->show();
}

void MainWindow::refreshTrayMenu()
{
  if (!m_trayMenu)
    return;
  using namespace deskflow::gui::brand;
  const auto dpr = devicePixelRatioF();
  const bool server = m_coreProcess.mode() == CoreMode::Server;
  const bool running = m_coreProcess.isStarted();
  const auto state = m_coreProcess.connectionState();
  const auto thisName = Settings::value(Settings::Core::ComputerName).toString();
  const auto mainName = Settings::value(Settings::Client::RemoteHost).toString();

  m_trayMenu->clear();

  // Headline (not clickable)
  QString headline;
  if (server)
    headline = running ? tr("Sharing from %1").arg(thisName) : tr("Sharing is off");
  else if (state == ConnectionState::Connected)
    headline = tr("Linked to %1").arg(mainName);
  else
    headline = running ? tr("Looking for %1…").arg(mainName) : tr("Not linked");
  auto *head = m_trayMenu->addAction(QIcon(icon(Icon::Bolt, 16, dpr, kAccent)), headline);
  head->setEnabled(false);

  // Computers on the desk with their state
  if (server) {
    bool any = false;
    for (const auto &computer : std::as_const(m_serverConfig).computers()) {
      if (computer.isNull() || computer.name() == thisName)
        continue;
      any = true;
      const bool linked = m_connectedClients.contains(computer.name());
      const QString status = linked ? tr("Linked") : running ? tr("Not answering") : tr("Waiting");
      auto *a = m_trayMenu->addAction(
          QIcon(icon(
              Icon::Monitor, 16, dpr,
              linked    ? kGreen
              : running ? kAccent
                        : kDim
          )),
          QStringLiteral("%1 — %2").arg(computer.name(), status)
      );
      a->setEnabled(false);
    }
    if (any)
      m_trayMenu->addSeparator();
  } else {
    m_trayMenu->addSeparator();
  }

  // Actions
  if (server)
    m_actionTrayToggle->setText(running ? tr("Pause sharing") : tr("Resume sharing"));
  else
    m_actionTrayToggle->setText(running ? tr("Unlink for now") : tr("Link again"));
  m_actionTrayToggle->setIcon(QIcon(icon(running ? Icon::Lock : Icon::Bolt, 16, dpr, kText)));
  m_trayMenu->addAction(m_actionTrayToggle);
  if (server) {
    m_actionTrayAdd->setIcon(QIcon(icon(Icon::Plus, 16, dpr, kText)));
    m_trayMenu->addAction(m_actionTrayAdd);
  }
  m_actionRestore->setIcon(QIcon(icon(Icon::Monitor, 16, dpr, kText)));
  m_actionRestore->setVisible(true);
  m_trayMenu->addAction(m_actionRestore);
  m_trayMenu->addSeparator();
  m_actionTrayQuit->setIcon(QIcon());
  m_trayMenu->addAction(m_actionTrayQuit);
}

void MainWindow::applyConfig()
{
  if (Settings::value(Settings::Gui::ShowVersionInTitle).toBool()) {
    setWindowTitle(QStringLiteral("%1 - %2").arg(kAppName, kDisplayVersion));
  } else {
    setWindowTitle(kAppName);
  }

  const auto host = Settings::value(Settings::Client::RemoteHost).toString();
  QSignalBlocker hostnameBlocker(ui->cbHostname);
  ui->cbHostname->clear();
  ui->cbHostname->addItems(Settings::value(Settings::Client::RecentRemoteHosts).toStringList());
  if (!host.isEmpty())
    ui->cbHostname->setCurrentText(host);

  updateFingerprintButton();
  setTrayIcon();

  if (const auto ip = Settings::value(Settings::Core::Interface).toString(); !ip.isEmpty()) {
    m_serverStartIPs = {ip};
    m_serverStartSuggestedIP = ip;
  }

  coreModeToggled(true);
}

void MainWindow::saveSettings() const
{
  if (ui->rbModeClient->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
  } else if (ui->rbModeServer->isChecked()) {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  }
  if (!ui->cbHostname->currentText().isEmpty()) {
    Settings::setValue(Settings::Client::RemoteHost, ui->cbHostname->currentText());
  }
  Settings::save();
}

void MainWindow::setTrayIcon()
{
  static const auto fallbackPath = QStringLiteral(":/icons/%1-%2/apps/64/%3");

  QString themeIcon = kRevFqdnName;
  if (!Settings::value(Settings::Gui::SymbolicTrayIcon).toBool()) {
    if (deskflow::platform::isMac())
      m_trayIcon->setIcon(QIcon::fromTheme(themeIcon));
    else
      m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, QStringLiteral("dark"), themeIcon)));
    return;
  }

  themeIcon.append(QStringLiteral("-symbolic"));

  if (deskflow::platform::isWindows()) {
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat
    );
    const QString theme = settings.value(QStringLiteral("SystemUsesLightTheme"), 1).toBool() ? QStringLiteral("light")
                                                                                             : QStringLiteral("dark");
    m_trayIcon->setIcon(QIcon(fallbackPath.arg(kAppId, theme, themeIcon)));
    return;
  }

  auto icon = QIcon::fromTheme(themeIcon, QIcon(fallbackPath.arg(kAppId, iconMode(), themeIcon)));
  icon.setIsMask(true);
  m_trayIcon->setIcon(icon);
}

void MainWindow::handleLogLine(const QString &line)
{
  m_logDock->appendLine(line);
  if (m_computerLog)
    m_computerLog->observe(line);
  if (m_deskView)
    m_deskView->checks()->appendLogLine(line);
}

void MainWindow::handleClientLimitReached(const QString &clientName)
{
  // One prompt per computer per session; the core keeps refusing quietly in the meantime.
  if (m_limitPromptedClients.contains(clientName))
    return;
  m_limitPromptedClients.insert(clientName);

  const auto &license = deskflow::License::current(true);
  const auto allowed = license.maxExtraComputers();
  if (m_actionClassicView && !m_actionClassicView->isChecked()) {
    using deskflow::gui::NoticeCard;
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Neutral)
        ->setTitle(tr("Room for one more?"))
        ->setBody(tr("%1 tried to join, but your %2 plan links this computer and %n other(s). "
                     "Lightning Desk Plus links up to five computers on one desk, with 14 days free to try. "
                     "Nothing else about Free changes.",
                     "", allowed)
                      .arg(clientName, license.isLicensed() ? license.tier() : tr("Free")))
        ->setButtons({tr("Not now"), tr("See Plus")});
    connect(card, &NoticeCard::answered, this, [this](int button, int) {
      m_deskView->dismissNotice();
      if (button == 1)
        QDesktopServices::openUrl(QUrl(QStringLiteral("%1?%2").arg(kUrlPlus, kUrlSourceQuery)));
    });
    showAndActivate();
    return;
  }
  QMessageBox box(this);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(tr("Room for one more?"));
  box.setText(tr("%1 tried to join, but your %2 plan links this computer and %n other(s).", "", allowed)
                  .arg(clientName, license.isLicensed() ? license.tier() : tr("Free")));
  box.setInformativeText(
      tr("Lightning Desk Plus links up to five computers on one desk, with 14 days free to try. "
         "Nothing else about Free changes.")
  );
  auto *learn = box.addButton(tr("See Plus"), QMessageBox::AcceptRole);
  box.addButton(tr("Not now"), QMessageBox::RejectRole);
  box.exec();
  if (box.clickedButton() == learn) {
    QDesktopServices::openUrl(QUrl(QStringLiteral("%1?%2").arg(kUrlPlus, kUrlSourceQuery)));
  }
}

void MainWindow::handleUnrecognisedClient(const QString &clientName)
{
  if (m_ignoredClients.contains(clientName)) {
    qDebug("ignoring %s:", qPrintable(clientName));
    return;
  }

  if (m_newClientPromptShowing || m_serverConfigDialogVisible)
    return;

  if (Settings::value(Settings::Server::ExternalConfig).toBool())
    return;

  if (m_serverConfig.isFull() || m_serverConfig.computerExists(clientName))
    return;

  m_newClientPromptShowing = true;

  showAndActivate();

  if (m_actionClassicView && !m_actionClassicView->isChecked()) {
    // Desk screen: ask where the new computer sits, inline, and place it straight away.
    using deskflow::gui::NoticeCard;
    QList<int> taken;
    const auto &list = std::as_const(m_serverConfig).computers();
    for (int i = 0; i < list.size(); ++i)
      if (!list[i].isNull())
        taken << i;
    const auto mainName = Settings::value(Settings::Core::ComputerName).toString();
    // 5 x 3 grid, main computer in the centre (index 7).
    const QList<NoticeCard::Choice> choices{
        {tr("Above"), 2, !taken.contains(2)},    {tr("Left"), 6, !taken.contains(6)},
        {tr("Right"), 8, !taken.contains(8)},    {tr("Below"), 12, !taken.contains(12)},
        {tr("Far left"), 5, !taken.contains(5)}, {tr("Far right"), 9, !taken.contains(9)}
    };
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Laptop, NoticeCard::Tone::Accent)
        ->setTitle(tr("%1 wants to join your desk").arg(clientName))
        ->setBody(tr("Pick the edge of %1's screen it sits next to. Move the mouse off that edge and it appears there. "
                     "You can move it later from its card.")
                      .arg(mainName))
        ->setChoices(choices)
        ->setButtons({tr("Not now"), tr("Add %1").arg(clientName)});
    connect(card, &NoticeCard::answered, this, [this, clientName](int button, int cell) {
      m_deskView->dismissNotice();
      m_newClientPromptShowing = false;
      if (button == 1 && cell >= 0) {
        m_serverConfig.placeClient(clientName, cell);
        m_serverConfig.commit();
        refreshDeskView();
        if (m_coreProcess.isStarted() && !m_coreProcess.reloadServerConfig())
          m_coreProcess.restart();
      } else {
        m_ignoredClients.insert(clientName);
      }
    });
    return;
  }

  if (deskflow::gui::messages::showNewClientPrompt(this, clientName)) {
    serverConnectionConfigureClient(clientName);
  } else {
    m_ignoredClients.insert(clientName);
  }

  m_newClientPromptShowing = false;
}

void MainWindow::handleConnectionRefused(deskflow::core::ConnectionRefusal reason)
{
  if (reason != deskflow::core::ConnectionRefusal::AlreadyConnected)
    return;

  if (!isVisible() || m_clientErrorVisible)
    return;

  m_clientErrorVisible = true;
  showAndActivate();

  const auto address = Settings::value(Settings::Client::RemoteHost).toString();
  if (auto *card = deskNotice()) {
    using deskflow::gui::NoticeCard;
    card->setIcon(deskflow::gui::brand::Icon::Laptop, NoticeCard::Tone::Danger)
        ->setTitle(tr("%1 turned this computer away").arg(address))
        ->setBody(
            tr("Either another computer with this name is already linked, or the main computer's plan "
               "already has as many computers as it allows (Free links the main computer and one other; "
               "Plus links up to five). Give this computer a different name in Settings, or see Plus.")
        )
        ->setButtons({tr("OK"), tr("See Plus")});
    connect(card, &NoticeCard::answered, this, [this](int button, int) {
      m_deskView->dismissNotice();
      m_clientErrorVisible = false;
      if (button == 1)
        QDesktopServices::openUrl(QUrl(QStringLiteral("%1?%2").arg(kUrlPlus, kUrlSourceQuery)));
    });
    return;
  }
  QMessageBox::warning(
      this, tr("%1 Connection Error").arg(kAppName),
      tr("<p>The main computer at '%1' turned this computer away.</p>"
         "<p>Either another computer with this name is already linked, or the main computer's plan "
         "already has as many computers as it allows (Free links the main computer and one other; "
         "Plus links up to five).</p>"
         "<p>Give this computer a different name, or <a href=\"%2\">see Plus</a>.</p>")
          .arg(address, kUrlPlus)
  );

  m_clientErrorVisible = false;
}

void MainWindow::handleMissingKeyboardLayouts(const QString &layouts)
{
  if (Settings::value(Settings::Gui::IgnoreMissingKeyboardLayouts).toBool())
    return;

  QMessageBox msgBox(this);
  msgBox.setIcon(QMessageBox::Warning);
  msgBox.setWindowTitle(tr("Missing Keyboard Layouts"));
  msgBox.setText(tr("<p>Keyboard layout support requires matching layouts on all computers. "
                    "The following layouts from the other computer are not installed on this computer:</p>"
                    "<p><b>%1</b></p>"
                    "<p>Please install them to enable support for these layouts.</p>")
                     .arg(layouts));

  if (auto *card = deskNotice()) {
    using deskflow::gui::NoticeCard;
    card->setIcon(deskflow::gui::brand::Icon::Keyboard, NoticeCard::Tone::Neutral)
        ->setTitle(tr("A keyboard layout is missing here"))
        ->setBody(tr("The other computer types with %1, which isn't installed on this one. Typing still works; "
                     "install the layout to match it exactly.")
                      .arg(layouts))
        ->setButtons({tr("Don't tell me again"), tr("OK")});
    connect(card, &NoticeCard::answered, this, [this](int button, int) {
      m_deskView->dismissNotice();
      if (button == 0)
        Settings::setValue(Settings::Gui::IgnoreMissingKeyboardLayouts, true);
    });
    return;
  }
  auto *checkBox = new QCheckBox(tr("Don't show this again"), &msgBox);
  msgBox.setCheckBox(checkBox);
  msgBox.exec();

  if (checkBox->isChecked()) {
    Settings::setValue(Settings::Gui::IgnoreMissingKeyboardLayouts, true);
  }
}

void MainWindow::handlePeerFingerprint(const QString &fingerprint)
{
  const auto sha256Text = QString(fingerprint).remove(':');
  const Fingerprint sha256 = {QCryptographicHash::Sha256, QByteArray::fromHex(sha256Text.toLatin1())};

  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  if ((isClient && m_checkedServers.contains(sha256Text)) || (!isClient && m_checkedClients.contains(sha256Text))) {
    qDebug("ignoring fingerprint, already handled");
    return;
  }

  FingerprintDatabase db;
  db.read(trustedFingerprintDatabase());

  if (db.isTrusted(sha256)) {
    qDebug("fingerprint is trusted");
    return;
  }

  if (isClient) {
    m_checkedServers.append(sha256Text);
    m_coreProcess.stop();
  } else {
    m_checkedClients.append(sha256Text);
  }

  // Trusting a peer, shared by the inline notice and the classic dialog.
  const auto trust = [this, sha256, sha256Text, isClient]() {
    // Remember the code in the connection log: a main's under its name, an extra's with the
    // next computer that links (it connects right after being trusted).
    QString code;
    const auto up = sha256Text.toUpper();
    for (int i = 0; i < 4 && i * 4 + 4 <= up.size(); ++i)
      code += (i ? QStringLiteral(" ") : QString()) + up.mid(i * 4, 4);
    if (m_computerLog) {
      if (isClient)
        m_computerLog->setFingerprint(Settings::value(Settings::Client::RemoteHost).toString(), code);
      else
        m_pendingFingerprint = code;
    }
    FingerprintDatabase db;
    db.read(trustedFingerprintDatabase());
    db.addTrusted(sha256);
    if (!db.write(trustedFingerprintDatabase())) {
      qCritical().noquote() << "unable to write fingerprint to db:" << trustedFingerprintDatabase();
      m_coreProcess.stop();
      return;
    }
    if (isClient) {
      m_checkedServers.removeAll(sha256Text);
      m_coreProcess.start();
    } else {
      m_checkedClients.removeAll(sha256Text);
    }
  };

  if (m_actionClassicView && !m_actionClassicView->isChecked()) {
    using deskflow::gui::NoticeCard;
    // The server learns the client's name only after the TLS handshake, so on this side the
    // notice shows the security code without a name.
    const auto peer = isClient ? Settings::value(Settings::Client::RemoteHost).toString() : QString();
    const auto shown = peer.isEmpty() ? tr("this computer") : peer;
    auto *card = m_deskView->notice();
    card->setIcon(deskflow::gui::brand::Icon::Laptop, NoticeCard::Tone::Accent)
        ->setTitle(isClient ? tr("Link to %1?").arg(shown) : tr("A new computer wants to join"))
        ->setBody(
            isClient ? tr("Your mouse and keyboard will reach this computer from %1. If that's not your main "
                          "computer, say no.")
                           .arg(shown)
                     : tr("If this is one of your computers, allow it and it joins your desk. If you weren't "
                          "expecting anything, say no — nothing connects until you do.")
        )
        ->setCode(sha256Text)
        ->setFootnote(tr("This is %1's security code. It never changes unless %1 makes a new key, so you'll only "
                         "be asked once.")
                          .arg(shown))
        ->setButtons(
            {tr("Not now"), isClient ? tr("Yes, link") : (peer.isEmpty() ? tr("Allow it") : tr("Allow %1").arg(peer))}
        );
    connect(card, &NoticeCard::answered, this, [this, trust](int button, int) {
      m_deskView->dismissNotice();
      if (button == 1)
        trust();
    });
    showAndActivate();
    return;
  }

  auto mode = isClient ? FingerprintDialogMode::Client : FingerprintDialogMode::Server;
  FingerprintDialog fingerprintDialog(this, m_fingerprint, mode, sha256);
  if (fingerprintDialog.exec() == QDialog::Accepted)
    trust();
}

bool MainWindow::maybeHideToTray()
{
  if (!Settings::value(Settings::Gui::CloseToTray).toBool()) {
    return false;
  }

  if (Settings::value(Settings::Gui::CloseReminder).toBool()) {
    // Desk mode: no reminder box; the tray icon and the Settings switch explain it.
    if (!m_actionClassicView || m_actionClassicView->isChecked())
      messages::showCloseReminder(this);
    Settings::setValue(Settings::Gui::CloseReminder, false);
  }
  Settings::setValue(Settings::Gui::WindowGeometry, geometry());
  qDebug() << "hiding to tray";
  hide();
  return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (event->spontaneous() && maybeHideToTray()) {
    event->ignore();
    return;
  }

  if (m_saveOnExit) {
    Settings::setValue(Settings::Gui::WindowGeometry, geometry());
    Settings::setValue(Settings::Gui::AutoStartCore, m_coreProcess.isStarted());
  }
  qDebug() << "quitting application";

  // any connected dock view acitons will be triggered
  // disconnect them before accepting the event
  disconnect(m_logDock->toggleViewAction(), &QAction::toggled, nullptr, nullptr);

  event->accept();
  QApplication::quit();
}

void MainWindow::showFirstConnectedMessage()
{
  if (Settings::value(Settings::Gui::ShownFirstConnectedMessage).toBool())
    return;
  if (m_actionClassicView && !m_actionClassicView->isChecked())
    return; // the Desk screen shows the link state itself
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  messages::showFirstConnectedMessage(this);
}

void MainWindow::updateStatus()
{
  using enum ProcessState;
  const auto connection = m_coreProcess.connectionState();
  const auto process = m_coreProcess.processState();
  const bool isServer = (m_coreProcess.mode() == CoreMode::Server);
  if (process == Stopped || process == Started) {
    updateNetworkInfo();
    ui->btnEditName->setVisible(process == Stopped);
  }
  m_statusBar->setStatus(connection, process, isServer);
}

void MainWindow::coreProcessStateChanged(ProcessState state)
{
  using enum ProcessState;
  updateStatus();
  if (state == Started) {
    qDebug() << "recording that core has started";
    Settings::setValue(Settings::Gui::AutoStartCore, true);
    const bool classicView = m_actionClassicView && m_actionClassicView->isChecked();
    if (classicView && m_coreProcess.mode() == CoreMode::Server &&
        !Settings::value(Settings::Gui::ShownServerFirstStartMessage).toBool()) {
      qDebug() << "starting core server for first time";
      messages::showFirstServerStartMessage(this);
      Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
    }
  }

  if (state == Started || state == Starting || state == RetryPending) {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(true);
    m_actionStartCore->setVisible(false);
    m_actionRestartCore->setVisible(true);
    m_actionStopCore->setEnabled(true);

    if (state == Starting) {
      saveSettings();
    }

  } else {
    disconnect(ui->btnToggleCore, &QPushButton::clicked, m_actionStopCore, &QAction::trigger);
    connect(ui->btnToggleCore, &QPushButton::clicked, m_actionStartCore, &QAction::trigger, Qt::UniqueConnection);

    ui->btnRestartCore->setEnabled(false);
    m_actionStartCore->setVisible(true);
    m_actionRestartCore->setVisible(false);
    m_actionStopCore->setEnabled(false);
  }
  updateModeControlLabels();
  if (m_deskView)
    m_deskView->setSharing(m_coreProcess.isStarted());
  refreshChecks();
  refreshTrayMenu();
}

void MainWindow::coreConnectionStateChanged(ConnectionState state)
{
  qDebug() << "core connection state changed:" << static_cast<int>(state);

  updateStatus();
  if (m_deskView) {
    m_deskView->setConnectionState(state);
    m_deskView->setSharing(m_coreProcess.isStarted());
    if (state == ConnectionState::Disconnected) {
      m_connectedClients.clear();
      if (m_computerLog)
        m_computerLog->allLeft();
      m_deskView->setConnectedClients({});
    }
    refreshChecks();
  }
  refreshTrayMenu();

  // always assume connection is not secure when connection changes
  // to anything except connected. the only way the padlock shows is
  // when the correct TLS version string is detected.
  if (state != ConnectionState::Connected) {
    secureSocket(false);
  } else {
    if (m_coreProcess.mode() == CoreMode::Client) {
      if (const auto host = ui->cbHostname->currentText(); !host.isEmpty()) {
        QStringList recentHosts = Settings::value(Settings::Client::RecentRemoteHosts).toStringList();
        if (recentHosts.isEmpty() || recentHosts.first() != host) {
          recentHosts.removeAll(host);
          recentHosts.prepend(host);
          while (recentHosts.size() > 10) {
            recentHosts.removeLast();
          }
          Settings::setValue(Settings::Client::RecentRemoteHosts, recentHosts);
          QSignalBlocker hostnameBlocker(ui->cbHostname);
          ui->cbHostname->clear();
          ui->cbHostname->addItems(recentHosts);
          ui->cbHostname->setCurrentText(host);
        }
      }
    }
    if (isVisible()) {
      showFirstConnectedMessage();
    }
  }
}

void MainWindow::updateFingerprintButton()
{
  m_statusBar->setBtnFingerprintVisible(TlsUtility::isEnabled() && !m_fingerprint.data.isEmpty());
}

void MainWindow::hide()
{
#ifdef Q_OS_MACOS
  macOSNativeHide();
#else
  QMainWindow::hide();
#endif
  m_actionRestore->setVisible(true);
  m_actionMinimize->setVisible(false);
}

void MainWindow::changeEvent(QEvent *e)
{
  QMainWindow::changeEvent(e);
  if (e->type() == QEvent::PaletteChange) {
    updateIconTheme();
    setWindowIcon(QIcon::fromTheme(kRevFqdnName));
    setTrayIcon();
  } else if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateModeControlLabels();
    updateNetworkInfo();
    updateStatus();
    serverClientsChanged({});
    updateText();
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
  if (obj != ui->lineEditName || event->type() != QEvent::KeyPress)
    return false;
  if (const auto keyEvent = static_cast<QKeyEvent *>(event); keyEvent->key() != Qt::Key_Escape)
    return false;
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  ui->lineEditName->setText(Settings::value(Settings::Core::ComputerName).toString());
  toggleCanRunCore(canRunCore());
  return true;
}

void MainWindow::updateText()
{
  m_menuFile->setTitle(tr("&File"));
  m_menuEdit->setTitle(tr("&Edit"));
  m_menuView->setTitle(tr("&View"));
  m_menuHelp->setTitle(tr("&Help"));

  m_actionMinimize->setText(tr("&Minimize to tray"));
  m_actionQuit->setText(tr("&Quit"));
  m_actionTrayQuit->setText(tr("&Quit"));
  //: %1 will be the replaced with the appname
  m_actionRestore->setText(tr("&Open %1").arg(kAppName));
  m_actionSettings->setText(tr("&Preferences"));
  m_actionStartCore->setText(tr("&Start"));
  m_actionRestartCore->setText(tr("Rest&art"));
  m_actionStopCore->setText(tr("S&top"));
  //: %1 will be the replaced with the appname
  m_actionAbout->setText(tr("About %1...").arg(kAppName));

  m_actionShowHelp->setText(tr("View &Help"));

  //: start / restart core shortcut
  m_actionStartCore->setShortcut(QKeySequence(tr("Ctrl+S")));
  m_actionRestartCore->setShortcut(QKeySequence(tr("Ctrl+S")));

  //: stop core shortcut
  m_actionStopCore->setShortcut(QKeySequence(tr("Ctrl+T")));

  if (deskflow::platform::isWindows()) {
    //: Quit shortcut
    m_actionQuit->setShortcut(QKeySequence(tr("Ctrl+Q")));
    m_actionTrayQuit->setShortcut(QKeySequence(tr("Ctrl+Q")));
  }
}

void MainWindow::showConfigureServer(const QString &message)
{
  ServerConfigDialog dialog(this, serverConfig());
  dialog.message(message);
  if ((dialog.exec() == QDialog::Accepted) && m_coreProcess.isStarted()) {
    m_coreProcess.restart();
  }
}

void MainWindow::showConfigureClient()
{
  ClientConfigDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    applyConfig();
    if (m_coreProcess.isStarted()) {
      m_coreProcess.restart();
    }
  }
}

void MainWindow::secureSocket(bool secureSocket)
{
  m_secureSocket = secureSocket;
  updateSecurityIcon(m_statusBar->securityIconVisible());
}

void MainWindow::updateComputerName()
{
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();
  ui->lblComputerName->setText(computerName);
  ui->lineEditName->setText(computerName);
  m_serverConfig.updateServerName();
}

void MainWindow::showAndActivate()
{
  const auto wasVisible = isVisible();
#ifdef Q_OS_MACOS
  forceAppActive();
#endif
  showNormal();
  raise();
  activateWindow();
  m_actionRestore->setVisible(false);
  m_actionMinimize->setVisible(true);
  if (!wasVisible)
    restoreWindow();
}

void MainWindow::showHostNameEditor()
{
  ui->lineEditName->show();
  ui->lblComputerName->hide();
  ui->btnEditName->hide();
  toggleCanRunCore(false);
  ui->lineEditName->setFocus();
}

void MainWindow::setHostName()
{
  ui->lineEditName->hide();
  ui->lblComputerName->show();
  ui->btnEditName->show();
  toggleCanRunCore(canRunCore());

  QString text = ui->lineEditName->text();
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();

  if (text == computerName)
    return;

  const bool isServer = ui->rbModeServer->isChecked();
  bool existingComputer = false;
  if (isServer)
    existingComputer = serverConfig().computerExists(text);

  if (!ui->lineEditName->hasAcceptableInput() || text.isEmpty() || existingComputer) {
    blockSignals(true);
    ui->lineEditName->setText(computerName);
    blockSignals(false);

    const auto title = tr("Invalid Computer Name");
    QString body;
    if (existingComputer) {
      body = tr("Computer name already exists");
    } else {
      body =
          tr("The name you have chosen is invalid.\n\n"
             "Valid names:\n"
             "• Use letters and numbers\n"
             "• May also use _ or -\n"
             "• Are between 1 and 255 characters");
    }
    QMessageBox::information(this, title, body);
    return;
  }

  ui->lblComputerName->setText(ui->lineEditName->text());
  Settings::setValue(Settings::Core::ComputerName, ui->lineEditName->text());
  if (isServer)
    serverConfig().updateServerName();
  applyConfig();
}

QString MainWindow::trustedFingerprintDatabase() const
{
  const bool isClient = m_coreProcess.mode() == CoreMode::Client;
  return isClient ? Settings::tlsTrustedServersDb() : Settings::tlsTrustedClientsDb();
}

bool MainWindow::generateCertificate()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  if (!QFile::exists(certificate) && !TlsUtility::generateCertificate()) {
    return false;
  }

  m_fingerprint = {QCryptographicHash::Sha256, TlsUtility::certFingerprint()};
  updateFingerprintButton();
  return true;
}

void MainWindow::serverClientsChanged(const QStringList &clients)
{
  if (m_coreProcess.mode() != CoreMode::Server || !m_coreProcess.isStarted())
    return;
  if (!m_pendingFingerprint.isEmpty() && m_computerLog) {
    for (const auto &name : clients) {
      if (!m_connectedClients.contains(name)) {
        m_computerLog->setFingerprint(name, m_pendingFingerprint);
        m_pendingFingerprint.clear();
        break;
      }
    }
  }
  m_connectedClients = clients;
  m_statusBar->setServerClients(clients);
  if (m_deskView) {
    m_deskView->setConnectedClients(clients);
    refreshChecks();
  }
  refreshTrayMenu();
}

void MainWindow::daemonIpcClientConnectionFailed()
{
  if (auto *card = deskNotice()) {
    using deskflow::gui::NoticeCard;
    card->setIcon(deskflow::gui::brand::Icon::Bolt, NoticeCard::Tone::Danger)
        ->setTitle(tr("The background service isn't answering"))
        ->setBody(
            tr("Lightning Desk's background service keeps sharing going when you're not logged in. It may "
               "still be starting. Try again in a moment, or restart this computer if it keeps happening.")
        )
        ->setButtons({tr("Later"), tr("Try again")});
    connect(card, &NoticeCard::answered, this, [this](int button, int) {
      m_deskView->dismissNotice();
      if (button == 1)
        m_coreProcess.retryDaemon();
    });
    return;
  }
  if (deskflow::gui::messages::showDaemonOffline(this)) {
    m_coreProcess.retryDaemon();
  }
}

void MainWindow::toggleCanRunCore(bool enableButtons)
{
  const bool isStarted = m_coreProcess.isStarted();
  ui->btnToggleCore->setEnabled(enableButtons);
  ui->btnRestartCore->setEnabled(enableButtons && isStarted);
  m_actionStartCore->setEnabled(enableButtons);
  m_actionStopCore->setEnabled(enableButtons && isStarted);
}

void MainWindow::remoteHostChanged(const QString &newRemoteHost)
{
  m_coreProcess.setAddress(newRemoteHost);
  toggleCanRunCore(!newRemoteHost.isEmpty() && ui->rbModeClient->isChecked());
  if (newRemoteHost.isEmpty()) {
    Settings::setValue(Settings::Client::RemoteHost);
  } else {
    Settings::setValue(Settings::Client::RemoteHost, newRemoteHost);
  }
}

void MainWindow::updateIpLabel(const QStringList &addresses)
{
  const auto mode = m_coreProcess.mode();
  if (mode == CoreMode::None) {
    return;
  }

  static const auto colorText = QStringLiteral(R"(<span style="color:%1;">%2</span>)");
  const bool serverStarted = m_coreProcess.isStarted();
  const bool fixedIP = !Settings::value(Settings::Core::Interface).toString().isEmpty();

  if (!fixedIP && addresses.isEmpty() && !serverStarted || (serverStarted && m_serverStartSuggestedIP.isEmpty())) {
    ui->lblIpAddresses->setText(colorText.arg(palette().linkVisited().color().name(), tr("No IP Detected")));
    ui->lblIpAddresses->setToolTip(tr("Unable to detect an IP address. Check your network connection is active."));
    return;
  }

  QString labelText = fixedIP ? tr("Using IP: ") : tr("Suggested IP: ");
  QString toolTipText = tr("<p>If connecting via the hostname fails, try %1</p>");

  const bool filterIpList = (serverStarted || fixedIP);
  const QRegularExpression ipListFilter(filterIpList ? QStringLiteral("(%1)").arg(m_serverStartIPs.join("|")) : "");
  const QStringList ipList = addresses.filter(ipListFilter);

  bool IPValid = true;
  if (filterIpList && (m_serverStartSuggestedIP != m_currentIpAddress) || !ipList.contains(m_serverStartSuggestedIP)) {
    IPValid = !ipList.isEmpty();
  }

  if (IPValid) {
    m_currentIpAddress = ipList.first();
    labelText.append(m_currentIpAddress);
  } else {
    labelText.append(colorText.arg(palette().linkVisited().color().name(), m_serverStartSuggestedIP));
    toolTipText.append(tr("\nA bound IP is now invalid, you may need to restart the server."));
  }

  if (ipList.count() < 2 || fixedIP) {
    toolTipText = toolTipText.arg(tr("the suggested IP."));
  } else {
    toolTipText = toolTipText.arg(tr("one of the following IPs:<br/>%1").arg(ipList.join("<br/>")));
  }

  ui->lblIpAddresses->setText(labelText);
  ui->lblIpAddresses->setToolTip(mode == CoreMode::Server ? toolTipText : QString());
}

void MainWindow::updateTimeoutDelay(int newDelay)
{
  m_statusBar->setConnectionInterval(newDelay);
}

void MainWindow::setHelpFilePath()
{
  const QString appPath = QCoreApplication::applicationDirPath();
  auto buildPath = QString("%1/../docs/HelpMain.md").arg(appPath);
  auto installPath = QString("%1/../share/doc/%2/HelpMain.md").arg(appPath, kAppId);
  if (deskflow::platform::isMac()) {
    installPath = QString("%1/../Resources/docs/HelpMain.md").arg(appPath);
    buildPath = QString("%1/../../../../docs/HelpMain.md").arg(appPath);
  } else if (deskflow::platform::isWindows()) {
    installPath = QString("%1/docs/HelpMain.md").arg(appPath);
  }

  buildPath = QDir::cleanPath(buildPath);
  installPath = QDir::cleanPath(installPath);

  if (QFile::exists(installPath))
    m_helpPath = QUrl::fromLocalFile(installPath);
  else if (QFile::exists(buildPath))
    m_helpPath = QUrl::fromLocalFile(buildPath);
  else
    m_helpPath = kUrlWiki;
}

void MainWindow::showHelpViewer() const
{
  if (m_helpPath.isLocalFile()) {
    HelpDialog dialogHelp(this->centralWidget(), m_helpPath);
    dialogHelp.exec();
  } else {
    QDesktopServices::openUrl(m_helpPath);
  }
}

bool MainWindow::canRunCore() const
{
  const auto mode = m_coreProcess.mode();
  const bool isServer = mode == Settings::CoreMode::Server;
  const bool isClient = mode == Settings::CoreMode::Client;
  return ((isServer || isClient) && (isClient && !ui->cbHostname->currentText().isEmpty()) || isServer);
}
