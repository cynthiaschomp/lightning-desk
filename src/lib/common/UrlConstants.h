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
const auto kUrlDownload = QStringLiteral("https://schomp.ai/lightning-desk/");
const auto kUrlPlus = QStringLiteral("https://schomp.ai/lightning-desk/#plus");
const auto kUrlWiki = QStringLiteral("%1#readme").arg(kUrlApp);
// Lightning Desk: a plain-text "newest version" file on schomp.ai (downloads are email-gated, so
// there are no public GitHub releases to ask). Checks are off by default (Settings.h).
const auto kUrlUpdateCheck = QStringLiteral("https://schomp.ai/lightning-desk/version.txt");
const auto kUrlReleaseNotes = QStringLiteral("https://schomp.ai/lightning-desk/#changes");
const auto kUrlSupportMail = QStringLiteral("support@schomp.ai");

#if defined(Q_OS_LINUX)
const auto kUrlGnomeTrayFix = QStringLiteral("https://extensions.gnome.org/extension/615/appindicator-support/");
#endif
