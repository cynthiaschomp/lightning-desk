/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/WlrVirtualInputProtocols.h"

#include <wayland-client-protocol.h>

namespace deskflow::wlr {

namespace {

// Per-message argument type tables: one entry per argument, the interface for
// object/new_id arguments and nullptr for everything else.
const wl_interface *const kNone[] = {nullptr, nullptr, nullptr, nullptr, nullptr};
const wl_interface *const kCreatePointer[] = {&wl_seat_interface, &virtualPointerInterface};
const wl_interface *const kCreatePointerWithOutput[] = {
    &wl_seat_interface, &wl_output_interface, &virtualPointerInterface
};
const wl_interface *const kCreateKeyboard[] = {&wl_seat_interface, &virtualKeyboardInterface};
const wl_interface *const kGetXdgOutput[] = {&xdgOutputInterface, &wl_output_interface};

// zwlr_virtual_pointer_v1, version 2
const wl_message kVirtualPointerRequests[] = {
    {"motion", "uff", const_cast<const wl_interface **>(kNone)},
    {"motion_absolute", "uuuuu", const_cast<const wl_interface **>(kNone)},
    {"button", "uuu", const_cast<const wl_interface **>(kNone)},
    {"axis", "uuf", const_cast<const wl_interface **>(kNone)},
    {"frame", "", const_cast<const wl_interface **>(kNone)},
    {"axis_source", "u", const_cast<const wl_interface **>(kNone)},
    {"axis_stop", "uu", const_cast<const wl_interface **>(kNone)},
    {"axis_discrete", "uufi", const_cast<const wl_interface **>(kNone)},
    {"destroy", "", const_cast<const wl_interface **>(kNone)},
};

// zwlr_virtual_pointer_manager_v1, version 2
const wl_message kVirtualPointerManagerRequests[] = {
    {"create_virtual_pointer", "?on", const_cast<const wl_interface **>(kCreatePointer)},
    {"destroy", "", const_cast<const wl_interface **>(kNone)},
    {"create_virtual_pointer_with_output", "2?o?on", const_cast<const wl_interface **>(kCreatePointerWithOutput)},
};

// zwp_virtual_keyboard_v1, version 1
const wl_message kVirtualKeyboardRequests[] = {
    {"keymap", "uhu", const_cast<const wl_interface **>(kNone)},
    {"key", "uuu", const_cast<const wl_interface **>(kNone)},
    {"modifiers", "uuuu", const_cast<const wl_interface **>(kNone)},
    {"destroy", "", const_cast<const wl_interface **>(kNone)},
};

// zwp_virtual_keyboard_manager_v1, version 1
const wl_message kVirtualKeyboardManagerRequests[] = {
    {"create_virtual_keyboard", "on", const_cast<const wl_interface **>(kCreateKeyboard)},
};

// zxdg_output_v1, version 3
const wl_message kXdgOutputRequests[] = {
    {"destroy", "", const_cast<const wl_interface **>(kNone)},
};
const wl_message kXdgOutputEvents[] = {
    {"logical_position", "ii", const_cast<const wl_interface **>(kNone)},
    {"logical_size", "ii", const_cast<const wl_interface **>(kNone)},
    {"done", "", const_cast<const wl_interface **>(kNone)},
    {"name", "2s", const_cast<const wl_interface **>(kNone)},
    {"description", "2s", const_cast<const wl_interface **>(kNone)},
};

// zxdg_output_manager_v1, version 3
const wl_message kXdgOutputManagerRequests[] = {
    {"destroy", "", const_cast<const wl_interface **>(kNone)},
    {"get_xdg_output", "no", const_cast<const wl_interface **>(kGetXdgOutput)},
};

} // namespace

const wl_interface virtualPointerInterface = {"zwlr_virtual_pointer_v1", 2, 9, kVirtualPointerRequests, 0, nullptr};

const wl_interface virtualPointerManagerInterface = {"zwlr_virtual_pointer_manager_v1", 2, 3,
                                                     kVirtualPointerManagerRequests,    0, nullptr};

const wl_interface virtualKeyboardInterface = {"zwp_virtual_keyboard_v1", 1, 4, kVirtualKeyboardRequests, 0, nullptr};

const wl_interface virtualKeyboardManagerInterface = {"zwp_virtual_keyboard_manager_v1", 1, 1,
                                                      kVirtualKeyboardManagerRequests,   0, nullptr};

const wl_interface xdgOutputInterface = {"zxdg_output_v1", 3, 1, kXdgOutputRequests, 5, kXdgOutputEvents};

const wl_interface xdgOutputManagerInterface = {"zxdg_output_manager_v1", 3, 2, kXdgOutputManagerRequests, 0, nullptr};

} // namespace deskflow::wlr
