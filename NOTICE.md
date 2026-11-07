# Notice of modification

Lightning Desk is a modified version of Deskflow
(https://github.com/deskflow/deskflow), copyright (C) 2024-2026 Deskflow Developers,
(C) 2012-2025 Synergy App Ltd / Symless Ltd, (C) 2009-2012 Nick Bolton,
(C) 2002-2009 Chris Schoeneman, and other contributors.

It is distributed under the same license, GPL-2.0-only with the OpenSSL exception
(see LICENSES/). Source code is available at
https://github.com/cynthiaschomp/lightning-desk.

Changes made by Schomp Technologies LLC, starting in 2026, include:

- Renamed the product to Lightning Desk, with new icons, installer artwork,
  package names and app identifiers.
- Added a first-run setup wizard (src/lib/gui/wizard/) with QR-code linking.
- Added client support for Wayland desktops that lack the RemoteDesktop portal,
  such as Hyprland and Sway (src/lib/platform/WlrVirtualInput*).
- Lightning Desk moves existing Deskflow settings over automatically the first
  time it starts.
- Build commit stamping, plus packaging and CI changes for the fork.

New and substantially changed files carry an additional
"SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC" line. The original
notices are left in place.
