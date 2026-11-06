/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// important: this is used for settings paths on some platforms,
// and must not be a url. qt automatically converts this to reverse domain
// notation (rdn), e.g. org.deskflow
const auto kOrgDomain = QStringLiteral("schomp.ai");

const auto kUrlSourceQuery = QStringLiteral("source=gui");
const auto kUrlApp = QStringLiteral("https://github.com/cynthiaschomp/lightning-desk");
const auto kUrlHelp = QStringLiteral("%1#readme").arg(kUrlApp);
const auto kUrlDownload = QStringLiteral("%1/releases").arg(kUrlApp);
const auto kUrlWiki = QStringLiteral("%1#readme").arg(kUrlApp);
// Lightning Desk: our own update feed arrives with the updater (roadmap Phase 4).
// Until then update checks are off by default (Settings.h) and never reach deskflow.org.
const auto kUrlUpdateCheck =
    QStringLiteral("https://api.github.com/repos/cynthiaschomp/lightning-desk/releases/latest");

#if defined(Q_OS_LINUX)
const auto kUrlGnomeTrayFix = QStringLiteral("https://extensions.gnome.org/extension/615/appindicator-support/");
#endif
