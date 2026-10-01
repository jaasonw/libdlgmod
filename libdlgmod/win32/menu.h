#pragma once

#include <windows.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace dialog_module::detail {

// Win32 handle ownership is confined to this native API boundary.
struct MenuDeleter {
  void operator()(HMENU menu) const noexcept { DestroyMenu(menu); }
};
struct WindowDeleter {
  void operator()(HWND window) const noexcept { DestroyWindow(window); }
};
using Menu = std::unique_ptr<std::remove_pointer_t<HMENU>, MenuDeleter>;
using Window = std::unique_ptr<std::remove_pointer_t<HWND>, WindowDeleter>;

[[nodiscard]] inline Menu make_menu(std::string_view items) {
  if (items.empty()) return {};
  Menu menu{CreatePopupMenu()};
  if (!menu) return {};

  UINT identifier = 1;  // TrackPopupMenuEx reserves zero for cancellation.
  while (true) {
    const auto end = items.find('|');
    const auto item = items.substr(0, end);
    if (item.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
      return {};
    std::wstring label;
    if (!item.empty()) {
      const int count =
          MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, item.data(),
                              static_cast<int>(item.size()), nullptr, 0);
      if (count == 0) return {};
      label.resize(static_cast<size_t>(count));
      if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, item.data(),
                              static_cast<int>(item.size()), label.data(),
                              count) != count)
        return {};
    }
    // Display ampersands literally rather than as accelerator markers.
    for (size_t i = 0; i < label.size(); ++i) {
      if (label.at(i) == L'&') label.insert(++i, 1, L'&');
    }
    if (!AppendMenuW(menu.get(), MF_STRING, identifier, label.c_str()))
      return {};
    if (end == std::string_view::npos) return menu;
    if (identifier == (std::numeric_limits<UINT>::max)()) return {};
    ++identifier;
    items.remove_prefix(end + 1);
  }
}

[[nodiscard]] inline double show_native_menu(const char* items, double fallback,
                                             HWND owner) noexcept try {
  if (!items) return fallback;
  const auto menu = make_menu(items);
  if (!menu) return fallback;
  POINT position{};
  if (!GetCursorPos(&position)) return fallback;

  Window temporary_owner;
  if (!IsWindow(owner)) {
    temporary_owner.reset(CreateWindowExW(
        WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, position.x, position.y, 1,
        1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    if (!temporary_owner) return fallback;
    owner = temporary_owner.get();
    ShowWindow(owner, SW_SHOWNORMAL);
  }
  SetForegroundWindow(owner);
  const auto selected = static_cast<UINT>(TrackPopupMenuEx(
      menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, position.x,
      position.y, owner, nullptr));
  PostMessageW(owner, WM_NULL, 0, 0);
  return selected == 0 ? fallback : static_cast<double>(selected - 1);
} catch (const std::bad_alloc&) {
  return fallback;
} catch (const std::length_error&) {
  return fallback;
}

}  // namespace dialog_module::detail
