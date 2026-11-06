/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <glib.h>

#include <cstdint>
#include <string>
#include <vector>

struct eis;
struct eis_client;
struct eis_seat;
struct eis_device;
struct eis_event;
struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_keyboard;
struct wl_output;
struct wl_proxy;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace deskflow {

class EiComputer;

/**
 * @brief Client-side input injection for Wayland compositors that have no
 * RemoteDesktop portal (Hyprland, Sway, river, labwc, Wayfire, ...).
 *
 * EiComputer (as a client) only knows how to talk libei to an EIS server,
 * which it normally gets from the RemoteDesktop portal. On wlroots-style
 * compositors that portal does not exist, so the client used to quit right
 * after connecting. This class runs a small in-process EIS server instead and
 * forwards what EiComputer sends to the compositor's own virtual input
 * protocols:
 *
 *  - zwlr_virtual_pointer_manager_v1 (pointer motion, buttons, scroll)
 *  - zwp_virtual_keyboard_manager_v1 (keys and modifiers)
 *  - zxdg_output_manager_v1 / wl_output (screen layout for absolute motion)
 *
 * Clipboard sharing uses wl-copy / wl-paste (wl-clipboard) when present.
 *
 * All callbacks run on the GLib main context owned by PortalRemoteDesktop.
 */
class WlrVirtualInput
{
public:
  struct Output
  {
    wl_output *output = nullptr;
    wl_proxy *xdgOutput = nullptr;
    uint32_t globalName = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t modeWidth = 0;
    int32_t modeHeight = 0;
    int32_t scale = 1;
    int32_t transform = 0;
    int32_t logicalX = 0;
    int32_t logicalY = 0;
    int32_t logicalWidth = 0;
    int32_t logicalHeight = 0;
    bool haveLogical = false;
  };

  /// Returns a running bridge, or nullptr when the compositor lacks the
  /// protocols (the caller then keeps its old behaviour).
  static WlrVirtualInput *create(EiComputer *computer);
  ~WlrVirtualInput();

  WlrVirtualInput(const WlrVirtualInput &) = delete;
  WlrVirtualInput &operator=(const WlrVirtualInput &) = delete;

  /// The client end of the in-process EIS socket; ownership passes to the caller.
  int takeClientFd();

  /// Push the shared clipboard (from the server) into the local Wayland clipboard.
  void claimClipboard();

  // Wayland listener entry points (public so the C callbacks can reach them).
  void onGlobal(wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
  void onGlobalRemove(uint32_t name);
  void onSeatCapabilities(uint32_t caps);
  void onKeymap(uint32_t format, int32_t fd, uint32_t size);
  void onOutputsChanged();

  std::vector<Output *> m_outputs;

private:
  explicit WlrVirtualInput(EiComputer *computer);
  bool init();
  bool dispatchWaylandOnce();
  void loadFallbackKeymap();

  void handleEisEvents();
  void handleSeatBind(eis_event *event);
  void addPointerDevice();
  void addAbsoluteDevice();
  void addKeyboardDevice();
  void removeDevice(eis_device *&device);
  void updateLayout(int32_t &minX, int32_t &minY, int32_t &width, int32_t &height) const;

  void sendModifiers();
  uint32_t nowMs() const;

  void setLocalClipboard();
  void startClipboardWatch();
  void handleClipboardLine(const std::string &base64);

  static gboolean eisReadyCallback(gint fd, GIOCondition condition, gpointer data);
  static gboolean waylandReadyCallback(gint fd, GIOCondition condition, gpointer data);
  static gboolean clipboardReadyCallback(GIOChannel *source, GIOCondition condition, gpointer data);

  EiComputer *m_computer;

  // Wayland
  wl_display *m_display = nullptr;
  wl_registry *m_registry = nullptr;
  wl_seat *m_seat = nullptr;
  wl_keyboard *m_keyboard = nullptr;
  wl_proxy *m_pointerManager = nullptr;
  wl_proxy *m_keyboardManager = nullptr;
  wl_proxy *m_xdgOutputManager = nullptr;
  wl_proxy *m_virtualPointer = nullptr;
  wl_proxy *m_virtualKeyboard = nullptr;
  guint m_waylandSource = 0;

  // Keymap shared by the EIS keyboard device and the virtual keyboard.
  std::string m_keymapText;
  xkb_context *m_xkbContext = nullptr;
  xkb_keymap *m_xkbKeymap = nullptr;
  xkb_state *m_xkbState = nullptr;

  // EIS
  eis *m_eis = nullptr;
  eis_client *m_client = nullptr;
  eis_seat *m_eisSeat = nullptr;
  eis_device *m_pointer = nullptr;
  eis_device *m_absolute = nullptr;
  eis_device *m_keyboardDevice = nullptr;
  bool m_wantAbsolute = false;
  int m_clientFd = -1;
  guint m_eisSource = 0;
  int32_t m_absWidth = 0;
  int32_t m_absHeight = 0;

  // Clipboard
  bool m_haveWlClipboard = false;
  GPid m_watchPid = 0;
  GIOChannel *m_watchChannel = nullptr;
  guint m_watchSource = 0;
  std::string m_watchBuffer;
  std::string m_lastClipboardText;
};

} // namespace deskflow
