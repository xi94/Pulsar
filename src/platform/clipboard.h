#pragma once

#include <string>
#include <string_view>

auto set_clipboard_text(std::string_view t_text) -> void;
auto set_clipboard_secret(std::string_view t_secret) -> void;
[[nodiscard]] auto clipboard_text() -> std::string;
[[nodiscard]] auto clipboard_has_text() -> bool;

[[nodiscard]] auto clipboard_sequence() -> u32;
auto clear_clipboard_if_unchanged(u32 t_sequence) -> void;
