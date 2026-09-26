#pragma once

#include <string>
#include <string_view>

void set_clipboard_text(std::string_view t_text);
void set_clipboard_secret(std::string_view t_secret);
std::string clipboard_text();
bool clipboard_has_text();

u32 clipboard_sequence();
void clear_clipboard_if_unchanged(u32 t_sequence);
