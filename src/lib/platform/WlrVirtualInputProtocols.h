/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

// Minimal client-side interface tables for the three Wayland protocols the
// wlroots input bridge needs. Written by hand from the protocol XML so the
// build needs only libwayland-client (no wayland-scanner, no protocol XML).
//
//  - wlr-virtual-pointer-unstable-v1  (zwlr_virtual_pointer_manager_v1 v2)
//  - virtual-keyboard-unstable-v1     (zwp_virtual_keyboard_manager_v1 v1)
//  - xdg-output-unstable-v1           (zxdg_output_manager_v1 v1)
//
// Request opcodes must match the order of requests in each protocol XML.

#include <wayland-client-core.h>

#include <cstdint>

namespace deskflow::wlr {

extern const wl_interface virtualPointerInterface;
extern const wl_interface virtualPointerManagerInterface;
extern const wl_interface virtualKeyboardInterface;
extern const wl_interface virtualKeyboardManagerInterface;
extern const wl_interface xdgOutputInterface;
extern const wl_interface xdgOutputManagerInterface;

inline constexpr const char *kVirtualPointerManagerName = "zwlr_virtual_pointer_manager_v1";
inline constexpr const char *kVirtualKeyboardManagerName = "zwp_virtual_keyboard_manager_v1";
inline constexpr const char *kXdgOutputManagerName = "zxdg_output_manager_v1";

// zwlr_virtual_pointer_v1 requests
enum VirtualPointerRequest : uint32_t
{
  VpMotion = 0,
  VpMotionAbsolute = 1,
  VpButton = 2,
  VpAxis = 3,
  VpFrame = 4,
  VpAxisSource = 5,
  VpAxisStop = 6,
  VpAxisDiscrete = 7,
  VpDestroy = 8,
};

// zwlr_virtual_pointer_manager_v1 requests
enum VirtualPointerManagerRequest : uint32_t
{
  VpmCreateVirtualPointer = 0,
  VpmDestroy = 1,
};

// zwp_virtual_keyboard_v1 requests
enum VirtualKeyboardRequest : uint32_t
{
  VkKeymap = 0,
  VkKey = 1,
  VkModifiers = 2,
  VkDestroy = 3,
};

// zwp_virtual_keyboard_manager_v1 requests
enum VirtualKeyboardManagerRequest : uint32_t
{
  VkmCreateVirtualKeyboard = 0,
};

// zxdg_output_manager_v1 requests
enum XdgOutputManagerRequest : uint32_t
{
  XomDestroy = 0,
  XomGetXdgOutput = 1,
};

// zxdg_output_v1 requests
enum XdgOutputRequest : uint32_t
{
  XoDestroy = 0,
};

/// Event listener for zxdg_output_v1 (same layout wayland-scanner would emit).
struct XdgOutputListener
{
  void (*logicalPosition)(void *data, wl_proxy *xdgOutput, int32_t x, int32_t y);
  void (*logicalSize)(void *data, wl_proxy *xdgOutput, int32_t width, int32_t height);
  void (*done)(void *data, wl_proxy *xdgOutput);
  void (*name)(void *data, wl_proxy *xdgOutput, const char *name);
  void (*description)(void *data, wl_proxy *xdgOutput, const char *description);
};

} // namespace deskflow::wlr
