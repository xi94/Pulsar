#pragma once

#include <string>
#include <string_view>

void set_clipboard_text(std::string_view t_text);
std::string clipboard_text();
bool clipboard_has_text();
