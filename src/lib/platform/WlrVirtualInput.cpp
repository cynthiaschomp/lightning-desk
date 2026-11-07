/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/WlrVirtualInput.h"

#include "base/Log.h"
#include "platform/EiClipboard.h"
#include "platform/EiComputer.h"
#include "platform/WlrVirtualInputProtocols.h"

#include <glib-unix.h>
#include <libeis.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <climits>
#include <csignal>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace deskflow {

using namespace deskflow::wlr;

namespace {

constexpr uint32_t kPointerManagerVersion = 2;
constexpr uint32_t kKeyboardManagerVersion = 1;
constexpr uint32_t kXdgOutputManagerVersion = 1;
constexpr uint32_t kSeatVersion = 5;
constexpr uint32_t kOutputVersion = 2;
constexpr int kWaylandRoundtrips = 4;
// One wheel detent in libei discrete units, and the matching Wayland axis value.
constexpr double kDiscreteStep = 120.0;
constexpr double kAxisPerDetent = 15.0;

// Writes text into an anonymous, sealed-size file the compositor can mmap.
int makeKeymapFd(const std::string &text)
{
  int fd = memfd_create("lightning-desk-keymap", MFD_CLOEXEC);
  if (fd < 0)
    return -1;
  const auto size = static_cast<off_t>(text.size() + 1);
  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }
  void *map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    close(fd);
    return -1;
  }
  memcpy(map, text.c_str(), text.size() + 1);
  munmap(map, size);
  return fd;
}

// ---- wl_registry
void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
  static_cast<WlrVirtualInput *>(data)->onGlobal(registry, name, interface, version);
}

void registryGlobalRemove(void *data, wl_registry *, uint32_t name)
{
  static_cast<WlrVirtualInput *>(data)->onGlobalRemove(name);
}

const wl_registry_listener kRegistryListener = {registryGlobal, registryGlobalRemove};

// ---- wl_keyboard (only the keymap matters; the rest are required no-ops)
void keyboardKeymap(void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size)
{
  static_cast<WlrVirtualInput *>(data)->onKeymap(format, fd, size);
}
void keyboardEnter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *)
{
}
void keyboardLeave(void *, wl_keyboard *, uint32_t, wl_surface *)
{
}
void keyboardKey(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t)
{
}
void keyboardModifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
}
void keyboardRepeatInfo(void *, wl_keyboard *, int32_t, int32_t)
{
}
const wl_keyboard_listener kKeyboardListener = {keyboardKeymap, keyboardEnter,     keyboardLeave,
                                                keyboardKey,    keyboardModifiers, keyboardRepeatInfo};

// ---- wl_seat
void seatCapabilities(void *data, wl_seat *, uint32_t caps)
{
  static_cast<WlrVirtualInput *>(data)->onSeatCapabilities(caps);
}
void seatName(void *, wl_seat *, const char *)
{
}
const wl_seat_listener kSeatListener = {seatCapabilities, seatName};

// ---- wl_output

void outputGeometry(
    void *data, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *, const char *,
    int32_t transform
)
{
  auto *o = static_cast<WlrVirtualInput::Output *>(data);
  o->x = x;
  o->y = y;
  o->transform = transform;
}
void outputMode(void *data, wl_output *, uint32_t flags, int32_t width, int32_t height, int32_t)
{
  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
    return;
  auto *o = static_cast<WlrVirtualInput::Output *>(data);
  o->modeWidth = width;
  o->modeHeight = height;
}
void outputDone(void *, wl_output *)
{
}
void outputScale(void *data, wl_output *, int32_t factor)
{
  static_cast<WlrVirtualInput::Output *>(data)->scale = std::max(1, factor);
}
const wl_output_listener kOutputListener = {outputGeometry, outputMode, outputDone, outputScale, nullptr, nullptr};

// ---- zxdg_output_v1
void xdgLogicalPosition(void *data, wl_proxy *, int32_t x, int32_t y)
{
  auto *o = static_cast<WlrVirtualInput::Output *>(data);
  o->logicalX = x;
  o->logicalY = y;
}
void xdgLogicalSize(void *data, wl_proxy *, int32_t w, int32_t h)
{
  auto *o = static_cast<WlrVirtualInput::Output *>(data);
  o->logicalWidth = w;
  o->logicalHeight = h;
  o->haveLogical = w > 0 && h > 0;
}
void xdgDone(void *, wl_proxy *)
{
}
void xdgName(void *, wl_proxy *, const char *)
{
}
void xdgDescription(void *, wl_proxy *, const char *)
{
}
const XdgOutputListener kXdgOutputListener = {xdgLogicalPosition, xdgLogicalSize, xdgDone, xdgName, xdgDescription};

bool programOnPath(const char *name)
{
  g_autofree gchar *path = g_find_program_in_path(name);
  return path != nullptr;
}

} // namespace

// ============================================================================

WlrVirtualInput *WlrVirtualInput::create(EiComputer *computer)
{
  auto *bridge = new WlrVirtualInput(computer);
  if (!bridge->init()) {
    delete bridge;
    return nullptr;
  }
  return bridge;
}

WlrVirtualInput::WlrVirtualInput(EiComputer *computer) : m_computer{computer}
{
  // nothing
}

WlrVirtualInput::~WlrVirtualInput()
{
  if (m_watchSource)
    g_source_remove(m_watchSource);
  if (m_watchChannel)
    g_io_channel_unref(m_watchChannel);
  if (m_watchPid > 0) {
    kill(m_watchPid, SIGTERM);
    g_spawn_close_pid(m_watchPid);
  }

  if (m_eisSource)
    g_source_remove(m_eisSource);
  if (m_waylandSource)
    g_source_remove(m_waylandSource);

  removeDevice(m_pointer);
  removeDevice(m_absolute);
  removeDevice(m_keyboardDevice);
  if (m_eisSeat)
    eis_seat_unref(m_eisSeat);
  if (m_client)
    eis_client_unref(m_client);
  if (m_eis)
    eis_unref(m_eis);
  if (m_clientFd >= 0)
    close(m_clientFd);

  if (m_xkbState)
    xkb_state_unref(m_xkbState);
  if (m_xkbKeymap)
    xkb_keymap_unref(m_xkbKeymap);
  if (m_xkbContext)
    xkb_context_unref(m_xkbContext);

  if (m_virtualPointer)
    wl_proxy_marshal_flags(
        m_virtualPointer, VpDestroy, nullptr, wl_proxy_get_version(m_virtualPointer), WL_MARSHAL_FLAG_DESTROY
    );
  if (m_virtualKeyboard)
    wl_proxy_marshal_flags(
        m_virtualKeyboard, VkDestroy, nullptr, wl_proxy_get_version(m_virtualKeyboard), WL_MARSHAL_FLAG_DESTROY
    );
  for (auto *o : m_outputs) {
    if (o->xdgOutput)
      wl_proxy_marshal_flags(
          o->xdgOutput, XoDestroy, nullptr, wl_proxy_get_version(o->xdgOutput), WL_MARSHAL_FLAG_DESTROY
      );
    if (o->output)
      wl_output_destroy(o->output);
    delete o;
  }
  m_outputs.clear();
  if (m_xdgOutputManager)
    wl_proxy_marshal_flags(
        m_xdgOutputManager, XomDestroy, nullptr, wl_proxy_get_version(m_xdgOutputManager), WL_MARSHAL_FLAG_DESTROY
    );
  if (m_pointerManager)
    wl_proxy_marshal_flags(
        m_pointerManager, VpmDestroy, nullptr, wl_proxy_get_version(m_pointerManager), WL_MARSHAL_FLAG_DESTROY
    );
  if (m_keyboardManager)
    wl_proxy_destroy(m_keyboardManager);
  if (m_keyboard)
    wl_keyboard_destroy(m_keyboard);
  if (m_seat)
    wl_seat_destroy(m_seat);
  if (m_registry)
    wl_registry_destroy(m_registry);
  if (m_display) {
    wl_display_flush(m_display);
    wl_display_disconnect(m_display);
  }
}

bool WlrVirtualInput::init()
{
  m_display = wl_display_connect(nullptr);
  if (!m_display) {
    LOG_WARN("wlroots input: no Wayland display to connect to");
    return false;
  }

  m_registry = wl_display_get_registry(m_display);
  wl_registry_add_listener(m_registry, &kRegistryListener, this);

  // Globals, then the objects they create (keymap, output details).
  for (int i = 0; i < kWaylandRoundtrips; ++i)
    wl_display_roundtrip(m_display);

  if (!m_pointerManager || !m_keyboardManager || !m_seat) {
    LOG_WARN(
        "wlroots input: compositor lacks virtual input protocols (pointer=%s keyboard=%s seat=%s)",
        m_pointerManager ? "yes" : "no", m_keyboardManager ? "yes" : "no", m_seat ? "yes" : "no"
    );
    return false;
  }

  m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (m_keymapText.empty())
    loadFallbackKeymap();
  if (m_keymapText.empty()) {
    LOG_ERR("wlroots input: could not obtain a keyboard layout");
    return false;
  }
  m_xkbKeymap = xkb_keymap_new_from_string(
      m_xkbContext, m_keymapText.c_str(), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS
  );
  if (!m_xkbKeymap) {
    LOG_ERR("wlroots input: keyboard layout did not compile");
    return false;
  }
  m_xkbState = xkb_state_new(m_xkbKeymap);

  // Virtual devices on the compositor side.
  m_virtualPointer = wl_proxy_marshal_flags(
      m_pointerManager, VpmCreateVirtualPointer, &virtualPointerInterface, wl_proxy_get_version(m_pointerManager), 0,
      m_seat, nullptr
  );
  m_virtualKeyboard = wl_proxy_marshal_flags(
      m_keyboardManager, VkmCreateVirtualKeyboard, &virtualKeyboardInterface, wl_proxy_get_version(m_keyboardManager),
      0, m_seat, nullptr
  );
  if (!m_virtualPointer || !m_virtualKeyboard) {
    LOG_ERR("wlroots input: compositor refused to create virtual devices");
    return false;
  }

  if (int fd = makeKeymapFd(m_keymapText); fd >= 0) {
    wl_proxy_marshal_flags(
        m_virtualKeyboard, VkKeymap, nullptr, wl_proxy_get_version(m_virtualKeyboard), 0,
        static_cast<uint32_t>(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1), fd, static_cast<uint32_t>(m_keymapText.size() + 1)
    );
    close(fd);
  }
  if (wl_display_roundtrip(m_display) < 0) {
    LOG_ERR("wlroots input: compositor closed the connection while creating virtual devices");
    return false;
  }

  // In-process EIS server; EiComputer connects to the other end of the socket.
  m_eis = eis_new(this);
  if (!m_eis || eis_setup_backend_fd(m_eis) != 0) {
    LOG_ERR("wlroots input: failed to start EIS");
    return false;
  }
  m_clientFd = eis_backend_fd_add_client(m_eis);
  if (m_clientFd < 0) {
    LOG_ERR("wlroots input: failed to create EIS client socket");
    return false;
  }

  m_eisSource = g_unix_fd_add(eis_get_fd(m_eis), G_IO_IN, eisReadyCallback, this);
  m_waylandSource = g_unix_fd_add(wl_display_get_fd(m_display), G_IO_IN, waylandReadyCallback, this);

  m_haveWlClipboard = programOnPath("wl-copy") && programOnPath("wl-paste");
  if (m_haveWlClipboard)
    startClipboardWatch();
  else
    LOG_WARN("wlroots input: wl-clipboard is not installed, copy and paste between computers is off");

  LOG_INFO("using wlroots virtual input (no RemoteDesktop portal on this desktop)");
  return true;
}

int WlrVirtualInput::takeClientFd()
{
  const int fd = m_clientFd;
  m_clientFd = -1;
  return fd;
}

// ---------------------------------------------------------------- Wayland

void WlrVirtualInput::onGlobal(wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
  if (strcmp(interface, kVirtualPointerManagerName) == 0 && !m_pointerManager) {
    m_pointerManager = static_cast<wl_proxy *>(
        wl_registry_bind(registry, name, &virtualPointerManagerInterface, std::min(version, kPointerManagerVersion))
    );
  } else if (strcmp(interface, kVirtualKeyboardManagerName) == 0 && !m_keyboardManager) {
    m_keyboardManager = static_cast<wl_proxy *>(
        wl_registry_bind(registry, name, &virtualKeyboardManagerInterface, std::min(version, kKeyboardManagerVersion))
    );
  } else if (strcmp(interface, kXdgOutputManagerName) == 0 && !m_xdgOutputManager) {
    m_xdgOutputManager = static_cast<wl_proxy *>(
        wl_registry_bind(registry, name, &xdgOutputManagerInterface, std::min(version, kXdgOutputManagerVersion))
    );
    for (auto *o : m_outputs) {
      if (!o->xdgOutput) {
        o->xdgOutput = wl_proxy_marshal_flags(
            m_xdgOutputManager, XomGetXdgOutput, &xdgOutputInterface, wl_proxy_get_version(m_xdgOutputManager), 0,
            nullptr, o->output
        );
        wl_proxy_add_listener(
            o->xdgOutput, reinterpret_cast<void (**)(void)>(const_cast<XdgOutputListener *>(&kXdgOutputListener)), o
        );
      }
    }
  } else if (strcmp(interface, wl_seat_interface.name) == 0 && !m_seat) {
    m_seat =
        static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, kSeatVersion)));
    wl_seat_add_listener(m_seat, &kSeatListener, this);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    auto *o = new Output;
    o->globalName = name;
    o->output = static_cast<wl_output *>(
        wl_registry_bind(registry, name, &wl_output_interface, std::min(version, kOutputVersion))
    );
    wl_output_add_listener(o->output, &kOutputListener, o);
    if (m_xdgOutputManager) {
      o->xdgOutput = wl_proxy_marshal_flags(
          m_xdgOutputManager, XomGetXdgOutput, &xdgOutputInterface, wl_proxy_get_version(m_xdgOutputManager), 0,
          nullptr, o->output
      );
      wl_proxy_add_listener(
          o->xdgOutput, reinterpret_cast<void (**)(void)>(const_cast<XdgOutputListener *>(&kXdgOutputListener)), o
      );
    }
    m_outputs.push_back(o);
    if (m_eis)
      onOutputsChanged();
  }
}

void WlrVirtualInput::onGlobalRemove(uint32_t name)
{
  auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [name](const Output *o) { return o->globalName == name; });
  if (it == m_outputs.end())
    return;
  auto *o = *it;
  if (o->xdgOutput)
    wl_proxy_marshal_flags(
        o->xdgOutput, XoDestroy, nullptr, wl_proxy_get_version(o->xdgOutput), WL_MARSHAL_FLAG_DESTROY
    );
  wl_output_destroy(o->output);
  delete o;
  m_outputs.erase(it);
  onOutputsChanged();
}

void WlrVirtualInput::onSeatCapabilities(uint32_t caps)
{
  // The real keyboard's keymap is the user's actual layout; reuse it.
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !m_keyboard) {
    m_keyboard = wl_seat_get_keyboard(m_seat);
    wl_keyboard_add_listener(m_keyboard, &kKeyboardListener, this);
  }
}

void WlrVirtualInput::onKeymap(uint32_t format, int32_t fd, uint32_t size)
{
  if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
    void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map != MAP_FAILED) {
      const auto *text = static_cast<const char *>(map);
      m_keymapText.assign(text, strnlen(text, size));
      munmap(map, size);
    }
  }
  close(fd);
}

void WlrVirtualInput::loadFallbackKeymap()
{
  // No physical keyboard reported: use the layout from XKB_DEFAULT_* (or "us").
  auto *keymap = xkb_keymap_new_from_names(m_xkbContext, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap)
    return;
  if (char *text = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1)) {
    m_keymapText = text;
    free(text);
  }
  xkb_keymap_unref(keymap);
  LOG_WARN("wlroots input: no keyboard layout from the compositor, using the default layout");
}

void WlrVirtualInput::onOutputsChanged()
{
  if (!m_absolute)
    return;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  updateLayout(x, y, w, h);
  if (w == m_absWidth && h == m_absHeight)
    return;
  LOG_INFO("wlroots input: screen layout changed to %dx%d", w, h);
  removeDevice(m_absolute);
  addAbsoluteDevice();
}

void WlrVirtualInput::updateLayout(int32_t &minX, int32_t &minY, int32_t &width, int32_t &height) const
{
  int32_t maxX = INT32_MIN;
  int32_t maxY = INT32_MIN;
  minX = INT32_MAX;
  minY = INT32_MAX;
  for (const auto *o : m_outputs) {
    int32_t ox = o->x;
    int32_t oy = o->y;
    int32_t ow = 0;
    int32_t oh = 0;
    if (o->haveLogical) {
      ox = o->logicalX;
      oy = o->logicalY;
      ow = o->logicalWidth;
      oh = o->logicalHeight;
    } else {
      const bool rotated = (o->transform % 2) == 1; // 90 / 270 (and flipped variants)
      ow = (rotated ? o->modeHeight : o->modeWidth) / o->scale;
      oh = (rotated ? o->modeWidth : o->modeHeight) / o->scale;
    }
    if (ow <= 0 || oh <= 0)
      continue;
    minX = std::min(minX, ox);
    minY = std::min(minY, oy);
    maxX = std::max(maxX, ox + ow);
    maxY = std::max(maxY, oy + oh);
  }
  if (minX == INT32_MAX) {
    // Nothing known yet: a sane default keeps absolute motion proportional.
    minX = 0;
    minY = 0;
    width = 1920;
    height = 1080;
    return;
  }
  width = maxX - minX;
  height = maxY - minY;
}

gboolean WlrVirtualInput::waylandReadyCallback(gint, GIOCondition condition, gpointer data)
{
  auto *self = static_cast<WlrVirtualInput *>(data);
  if (condition & (G_IO_HUP | G_IO_ERR)) {
    LOG_ERR("wlroots input: lost the Wayland connection");
    self->m_waylandSource = 0;
    return G_SOURCE_REMOVE;
  }
  if (!self->dispatchWaylandOnce()) {
    self->m_waylandSource = 0;
    return G_SOURCE_REMOVE;
  }
  wl_display_flush(self->m_display);
  return G_SOURCE_CONTINUE;
}

bool WlrVirtualInput::dispatchWaylandOnce()
{
  if (wl_display_dispatch(m_display) < 0) {
    LOG_ERR("wlroots input: Wayland connection error");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------- EIS

gboolean WlrVirtualInput::eisReadyCallback(gint, GIOCondition, gpointer data)
{
  static_cast<WlrVirtualInput *>(data)->handleEisEvents();
  return G_SOURCE_CONTINUE;
}

uint32_t WlrVirtualInput::nowMs() const
{
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void WlrVirtualInput::handleEisEvents()
{
  eis_dispatch(m_eis);

  bool flush = false;
  eis_event *event = nullptr;
  while ((event = eis_get_event(m_eis)) != nullptr) {
    const auto type = eis_event_get_type(event);
    auto *device = eis_event_get_device(event);
    const auto time = static_cast<uint32_t>(eis_event_get_time(event) / 1000);

    switch (type) {
    case EIS_EVENT_CLIENT_CONNECT: {
      auto *client = eis_event_get_client(event);
      if (m_client || !eis_client_is_sender(client)) {
        eis_client_disconnect(client);
        break;
      }
      m_client = eis_client_ref(client);
      eis_client_connect(client);
      m_eisSeat = eis_client_new_seat(client, "lightning-desk");
      eis_seat_configure_capability(m_eisSeat, EIS_DEVICE_CAP_POINTER);
      eis_seat_configure_capability(m_eisSeat, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
      eis_seat_configure_capability(m_eisSeat, EIS_DEVICE_CAP_KEYBOARD);
      eis_seat_configure_capability(m_eisSeat, EIS_DEVICE_CAP_BUTTON);
      eis_seat_configure_capability(m_eisSeat, EIS_DEVICE_CAP_SCROLL);
      eis_seat_add(m_eisSeat);
      LOG_DEBUG("wlroots input: input client connected");
      break;
    }
    case EIS_EVENT_CLIENT_DISCONNECT: {
      auto *client = eis_event_get_client(event);
      if (client == m_client) {
        removeDevice(m_pointer);
        removeDevice(m_absolute);
        removeDevice(m_keyboardDevice);
        if (m_eisSeat)
          m_eisSeat = eis_seat_unref(m_eisSeat);
        m_client = eis_client_unref(m_client);
      }
      eis_client_disconnect(client);
      LOG_DEBUG("wlroots input: input client disconnected");
      break;
    }
    case EIS_EVENT_SEAT_BIND:
      handleSeatBind(event);
      break;
    case EIS_EVENT_DEVICE_CLOSED:
      if (device == m_pointer)
        removeDevice(m_pointer);
      else if (device == m_absolute)
        removeDevice(m_absolute);
      else if (device == m_keyboardDevice)
        removeDevice(m_keyboardDevice);
      break;

    case EIS_EVENT_POINTER_MOTION:
      wl_proxy_marshal_flags(
          m_virtualPointer, VpMotion, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
          wl_fixed_from_double(eis_event_pointer_get_dx(event)), wl_fixed_from_double(eis_event_pointer_get_dy(event))
      );
      break;
    case EIS_EVENT_POINTER_MOTION_ABSOLUTE: {
      const double x = std::clamp(eis_event_pointer_get_absolute_x(event), 0.0, static_cast<double>(m_absWidth - 1));
      const double y = std::clamp(eis_event_pointer_get_absolute_y(event), 0.0, static_cast<double>(m_absHeight - 1));
      wl_proxy_marshal_flags(
          m_virtualPointer, VpMotionAbsolute, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
          static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(m_absWidth),
          static_cast<uint32_t>(m_absHeight)
      );
      break;
    }
    case EIS_EVENT_BUTTON_BUTTON:
      wl_proxy_marshal_flags(
          m_virtualPointer, VpButton, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
          eis_event_button_get_button(event),
          static_cast<uint32_t>(
              eis_event_button_get_is_press(event) ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED
          )
      );
      break;
    case EIS_EVENT_SCROLL_DELTA: {
      const double dx = eis_event_scroll_get_dx(event);
      const double dy = eis_event_scroll_get_dy(event);
      wl_proxy_marshal_flags(
          m_virtualPointer, VpAxisSource, nullptr, wl_proxy_get_version(m_virtualPointer), 0,
          static_cast<uint32_t>(WL_POINTER_AXIS_SOURCE_CONTINUOUS)
      );
      if (dy != 0.0)
        wl_proxy_marshal_flags(
            m_virtualPointer, VpAxis, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
            static_cast<uint32_t>(WL_POINTER_AXIS_VERTICAL_SCROLL), wl_fixed_from_double(dy)
        );
      if (dx != 0.0)
        wl_proxy_marshal_flags(
            m_virtualPointer, VpAxis, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
            static_cast<uint32_t>(WL_POINTER_AXIS_HORIZONTAL_SCROLL), wl_fixed_from_double(dx)
        );
      break;
    }
    case EIS_EVENT_SCROLL_DISCRETE: {
      const int32_t dx = eis_event_scroll_get_discrete_dx(event);
      const int32_t dy = eis_event_scroll_get_discrete_dy(event);
      wl_proxy_marshal_flags(
          m_virtualPointer, VpAxisSource, nullptr, wl_proxy_get_version(m_virtualPointer), 0,
          static_cast<uint32_t>(WL_POINTER_AXIS_SOURCE_WHEEL)
      );
      const auto sendDiscrete = [this, time](uint32_t axis, int32_t v120) {
        if (v120 == 0)
          return;
        const double detents = v120 / kDiscreteStep;
        const auto steps = static_cast<int32_t>(detents >= 0 ? std::max(1.0, detents) : std::min(-1.0, detents));
        wl_proxy_marshal_flags(
            m_virtualPointer, VpAxisDiscrete, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time, axis,
            wl_fixed_from_double(detents * kAxisPerDetent), steps
        );
      };
      sendDiscrete(WL_POINTER_AXIS_VERTICAL_SCROLL, dy);
      sendDiscrete(WL_POINTER_AXIS_HORIZONTAL_SCROLL, dx);
      break;
    }
    case EIS_EVENT_SCROLL_STOP:
    case EIS_EVENT_SCROLL_CANCEL:
      if (eis_event_scroll_get_stop_y(event))
        wl_proxy_marshal_flags(
            m_virtualPointer, VpAxisStop, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
            static_cast<uint32_t>(WL_POINTER_AXIS_VERTICAL_SCROLL)
        );
      if (eis_event_scroll_get_stop_x(event))
        wl_proxy_marshal_flags(
            m_virtualPointer, VpAxisStop, nullptr, wl_proxy_get_version(m_virtualPointer), 0, time,
            static_cast<uint32_t>(WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        );
      break;
    case EIS_EVENT_KEYBOARD_KEY: {
      const uint32_t key = eis_event_keyboard_get_key(event);
      const bool down = eis_event_keyboard_get_key_is_press(event);
      wl_proxy_marshal_flags(
          m_virtualKeyboard, VkKey, nullptr, wl_proxy_get_version(m_virtualKeyboard), 0, time, key,
          static_cast<uint32_t>(down ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED)
      );
      // evdev code + 8 = XKB keycode
      xkb_state_update_key(m_xkbState, key + 8, down ? XKB_KEY_DOWN : XKB_KEY_UP);
      sendModifiers();
      flush = true;
      break;
    }
    case EIS_EVENT_FRAME:
      if (device == m_pointer || device == m_absolute)
        wl_proxy_marshal_flags(m_virtualPointer, VpFrame, nullptr, wl_proxy_get_version(m_virtualPointer), 0);
      flush = true;
      break;
    case EIS_EVENT_DEVICE_START_EMULATING:
    case EIS_EVENT_DEVICE_STOP_EMULATING:
    default:
      break;
    }

    eis_event_unref(event);
  }

  if (flush)
    wl_display_flush(m_display);
}

void WlrVirtualInput::handleSeatBind(eis_event *event)
{
  const bool pointer = eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER);
  const bool absolute = eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
  const bool keyboard = eis_event_seat_has_capability(event, EIS_DEVICE_CAP_KEYBOARD);

  if (pointer && !m_pointer)
    addPointerDevice();
  else if (!pointer && m_pointer)
    removeDevice(m_pointer);

  m_wantAbsolute = absolute;
  if (absolute && !m_absolute)
    addAbsoluteDevice();
  else if (!absolute && m_absolute)
    removeDevice(m_absolute);

  if (keyboard && !m_keyboardDevice)
    addKeyboardDevice();
  else if (!keyboard && m_keyboardDevice)
    removeDevice(m_keyboardDevice);
}

void WlrVirtualInput::addPointerDevice()
{
  m_pointer = eis_seat_new_device(m_eisSeat);
  eis_device_configure_name(m_pointer, "Lightning Desk pointer");
  eis_device_configure_type(m_pointer, EIS_DEVICE_TYPE_VIRTUAL);
  eis_device_configure_capability(m_pointer, EIS_DEVICE_CAP_POINTER);
  eis_device_configure_capability(m_pointer, EIS_DEVICE_CAP_BUTTON);
  eis_device_configure_capability(m_pointer, EIS_DEVICE_CAP_SCROLL);
  eis_device_add(m_pointer);
  eis_device_resume(m_pointer);
}

void WlrVirtualInput::addAbsoluteDevice()
{
  int32_t x;
  int32_t y;
  updateLayout(x, y, m_absWidth, m_absHeight);

  m_absolute = eis_seat_new_device(m_eisSeat);
  eis_device_configure_name(m_absolute, "Lightning Desk absolute pointer");
  eis_device_configure_type(m_absolute, EIS_DEVICE_TYPE_VIRTUAL);
  eis_device_configure_capability(m_absolute, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
  eis_device_configure_capability(m_absolute, EIS_DEVICE_CAP_BUTTON);
  eis_device_configure_capability(m_absolute, EIS_DEVICE_CAP_SCROLL);
  // One region covering the whole desktop; the compositor maps the virtual
  // pointer's absolute motion onto its full output layout the same way.
  auto *region = eis_device_new_region(m_absolute);
  eis_region_set_offset(region, 0, 0);
  eis_region_set_size(region, static_cast<uint32_t>(m_absWidth), static_cast<uint32_t>(m_absHeight));
  eis_region_add(region);
  eis_region_unref(region);
  eis_device_add(m_absolute);
  eis_device_resume(m_absolute);
}

void WlrVirtualInput::addKeyboardDevice()
{
  m_keyboardDevice = eis_seat_new_device(m_eisSeat);
  eis_device_configure_name(m_keyboardDevice, "Lightning Desk keyboard");
  eis_device_configure_type(m_keyboardDevice, EIS_DEVICE_TYPE_VIRTUAL);
  eis_device_configure_capability(m_keyboardDevice, EIS_DEVICE_CAP_KEYBOARD);
  if (int fd = makeKeymapFd(m_keymapText); fd >= 0) {
    // Same layout as the virtual keyboard, so EiComputer picks the right keycodes.
    auto *keymap = eis_device_new_keymap(m_keyboardDevice, EIS_KEYMAP_TYPE_XKB, fd, m_keymapText.size() + 1);
    close(fd);
    if (keymap) {
      eis_keymap_add(keymap);
      eis_keymap_unref(keymap);
    }
  }
  eis_device_add(m_keyboardDevice);
  eis_device_resume(m_keyboardDevice);
}

void WlrVirtualInput::removeDevice(eis_device *&device)
{
  if (!device)
    return;
  eis_device_remove(device);
  device = eis_device_unref(device);
}

void WlrVirtualInput::sendModifiers()
{
  wl_proxy_marshal_flags(
      m_virtualKeyboard, VkModifiers, nullptr, wl_proxy_get_version(m_virtualKeyboard), 0,
      static_cast<uint32_t>(xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_DEPRESSED)),
      static_cast<uint32_t>(xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LATCHED)),
      static_cast<uint32_t>(xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LOCKED)),
      static_cast<uint32_t>(xkb_state_serialize_layout(m_xkbState, XKB_STATE_LAYOUT_EFFECTIVE))
  );
}

// ---------------------------------------------------------------- Clipboard

void WlrVirtualInput::claimClipboard()
{
  // Called from the core's event thread; do the work on the GLib thread that
  // owns the rest of this object.
  if (!m_haveWlClipboard)
    return;
  g_idle_add(
      [](gpointer data) {
        static_cast<WlrVirtualInput *>(data)->setLocalClipboard();
        return G_SOURCE_REMOVE;
      },
      this
  );
}

void WlrVirtualInput::setLocalClipboard()
{

  auto *cache = m_computer->getClipboardCache();
  if (!cache)
    return;
  cache->open(0);
  const bool hasText = cache->has(IClipboard::Format::Text);
  std::string text = hasText ? cache->get(IClipboard::Format::Text) : std::string();
  cache->close();
  if (!hasText || text == m_lastClipboardText)
    return;

  // g_spawn closes every other descriptor in the child, so wl-copy never
  // inherits the network socket.
  const gchar *argv[] = {"wl-copy", "--type", "text/plain;charset=utf-8", nullptr};
  GPid pid = 0;
  gint in = -1;
  g_autoptr(GError) error = nullptr;
  if (!g_spawn_async_with_pipes(
          nullptr, const_cast<gchar **>(argv), nullptr,
          static_cast<GSpawnFlags>(
              G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL
          ),
          nullptr, nullptr, &pid, &in, nullptr, nullptr, &error
      )) {
    LOG_WARN("clipboard: could not run wl-copy: %s", error->message);
    return;
  }
  g_child_watch_add(pid, [](GPid p, gint, gpointer) { g_spawn_close_pid(p); }, nullptr);

  size_t written = 0;
  while (written < text.size()) {
    const auto n = write(in, text.data() + written, text.size() - written);
    if (n <= 0)
      break;
    written += static_cast<size_t>(n);
  }
  close(in);
  m_lastClipboardText = std::move(text);
  LOG_DEBUG("clipboard: set local clipboard (%zu bytes)", written);
}

void WlrVirtualInput::startClipboardWatch()
{
  // wl-paste runs the command once per clipboard change; base64 keeps each
  // change on exactly one line no matter what the text contains.
  const gchar *argv[] = {"wl-paste", "--no-newline", "--type",           "text", "--watch",
                         "sh",       "-c",           "base64 -w0; echo", nullptr};
  gint out = -1;
  g_autoptr(GError) error = nullptr;
  if (!g_spawn_async_with_pipes(
          nullptr, const_cast<gchar **>(argv), nullptr,
          static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL),
          nullptr, nullptr, &m_watchPid, nullptr, &out, nullptr, &error
      )) {
    LOG_WARN("clipboard: could not watch the local clipboard: %s", error->message);
    m_watchPid = 0;
    return;
  }
  m_watchChannel = g_io_channel_unix_new(out);
  g_io_channel_set_close_on_unref(m_watchChannel, true);
  g_io_channel_set_encoding(m_watchChannel, nullptr, nullptr);
  g_io_channel_set_flags(m_watchChannel, G_IO_FLAG_NONBLOCK, nullptr);
  m_watchSource = g_io_add_watch(
      m_watchChannel, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR), clipboardReadyCallback, this
  );
}

gboolean WlrVirtualInput::clipboardReadyCallback(GIOChannel *source, GIOCondition condition, gpointer data)
{
  auto *self = static_cast<WlrVirtualInput *>(data);
  gchar buffer[4096];
  gsize got = 0;
  while (g_io_channel_read_chars(source, buffer, sizeof(buffer), &got, nullptr) == G_IO_STATUS_NORMAL && got > 0) {
    self->m_watchBuffer.append(buffer, got);
  }
  std::string::size_type nl;
  while ((nl = self->m_watchBuffer.find('\n')) != std::string::npos) {
    const auto line = self->m_watchBuffer.substr(0, nl);
    self->m_watchBuffer.erase(0, nl + 1);
    self->handleClipboardLine(line);
  }
  if (condition & (G_IO_HUP | G_IO_ERR)) {
    LOG_WARN("clipboard: local clipboard watcher stopped");
    self->m_watchSource = 0;
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

void WlrVirtualInput::handleClipboardLine(const std::string &base64)
{
  if (base64.empty())
    return;
  gsize len = 0;
  g_autofree guchar *decoded = g_base64_decode(base64.c_str(), &len);
  std::string text(reinterpret_cast<const char *>(decoded), len);
  while (!text.empty() && text.back() == '\0')
    text.pop_back();
  if (text.empty() || text == m_lastClipboardText)
    return; // our own wl-copy echoing back, or nothing useful

  const auto maxBytes = static_cast<size_t>(m_computer->maximumClipboardSize()) * 1024;
  if (text.size() > maxBytes) {
    LOG_INFO("clipboard: local copy is larger than the allowed size, not sharing it");
    return;
  }

  m_lastClipboardText = text;
  auto *cache = m_computer->getClipboardCache();
  cache->open(0);
  cache->empty();
  cache->add(IClipboard::Format::Text, text);
  cache->close();
  m_computer->sendClipboardEvent(EventTypes::ClipboardGrabbed, kClipboardClipboard);
  LOG_DEBUG("clipboard: shared local copy (%zu bytes)", text.size());
}

} // namespace deskflow
