#include "ui/theme.h"

#include <array>
#include <bit>
#include <cmath>

#include "core/animation.h"

namespace {
constexpr float K_FADE_RATE = 9.0f;

constexpr Theme K_DARK{
	.window         = {18, 18, 20, 255},
	.chrome         = {24, 24, 27, 255},
	.chrome_seam    = {46, 46, 50, 255},
	.surface        = {26, 26, 29, 255},
	.popup          = {32, 32, 36, 255},
	.field          = {20, 20, 23, 255},
	.control        = {42, 42, 47, 255},
	.control_hover  = {56, 56, 62, 255},
	.border         = {64, 64, 70, 255},
	.separator      = {50, 50, 56, 255},
	.row_hover      = {38, 38, 43, 255},
	.row_selected   = {52, 52, 58, 255},
	.track          = {70, 70, 76, 255},
	.text           = {232, 232, 236, 255},
	.text_dim       = {150, 150, 158, 255},
	.text_faint     = {108, 108, 116, 255},
	.scroll_thumb   = {120, 120, 128, 190},
	.scrim          = {0, 0, 0, 150},
	.shadow         = {0, 0, 0, 255},
	.success        = {80, 200, 120, 255},
	.error          = {220, 90, 80, 255},
	.default_accent = {203, 166, 247, 255},
};

constexpr Theme K_LIGHT{
	.window         = {236, 237, 241, 255},
	.chrome         = {248, 248, 250, 255},
	.chrome_seam    = {220, 221, 227, 255},
	.surface        = {252, 252, 253, 255},
	.popup          = {255, 255, 255, 255},
	.field          = {243, 244, 247, 255},
	.control        = {233, 234, 239, 255},
	.control_hover  = {221, 223, 230, 255},
	.border         = {210, 212, 220, 255},
	.separator      = {228, 229, 235, 255},
	.row_hover      = {242, 243, 247, 255},
	.row_selected   = {230, 231, 238, 255},
	.track          = {196, 198, 208, 255},
	.text           = {24, 25, 30, 255},
	.text_dim       = {92, 95, 108, 255},
	.text_faint     = {146, 149, 162, 255},
	.scroll_thumb   = {150, 152, 165, 170},
	.scrim          = {18, 20, 32, 80},
	.shadow         = {30, 34, 60, 255},
	.success        = {28, 150, 82, 255},
	.error          = {206, 60, 52, 255},
	.default_accent = {88, 72, 212, 255},
};

constexpr Theme K_FOREST{
	.window         = {17, 36, 26, 255},
	.chrome         = {20, 42, 31, 255},
	.chrome_seam    = {40, 74, 57, 255},
	.surface        = {23, 47, 35, 255},
	.popup          = {28, 56, 42, 255},
	.field          = {15, 33, 24, 255},
	.control        = {36, 70, 53, 255},
	.control_hover  = {46, 86, 66, 255},
	.border         = {54, 96, 74, 255},
	.separator      = {38, 72, 55, 255},
	.row_hover      = {31, 62, 47, 255},
	.row_selected   = {42, 80, 61, 255},
	.track          = {60, 104, 81, 255},
	.text           = {228, 244, 234, 255},
	.text_dim       = {150, 188, 166, 255},
	.text_faint     = {104, 142, 120, 255},
	.scroll_thumb   = {116, 166, 138, 190},
	.scrim          = {0, 12, 6, 150},
	.shadow         = {0, 10, 4, 255},
	.success        = {110, 226, 150, 255},
	.error          = {232, 104, 92, 255},
	.default_accent = {88, 210, 136, 255},
};

constexpr Theme K_OCEAN{
	.window         = {5, 37, 41, 255},
	.chrome         = {7, 44, 49, 255},
	.chrome_seam    = {18, 66, 72, 255},
	.surface        = {9, 49, 54, 255},
	.popup          = {12, 57, 63, 255},
	.field          = {5, 40, 44, 255},
	.control        = {17, 70, 77, 255},
	.control_hover  = {25, 87, 95, 255},
	.border         = {28, 88, 96, 255},
	.separator      = {19, 66, 73, 255},
	.row_hover      = {14, 60, 66, 255},
	.row_selected   = {21, 78, 86, 255},
	.track          = {33, 95, 103, 255},
	.text           = {222, 241, 243, 255},
	.text_dim       = {134, 178, 184, 255},
	.text_faint     = {88, 134, 140, 255},
	.scroll_thumb   = {104, 158, 165, 190},
	.scrim          = {0, 12, 14, 150},
	.shadow         = {0, 10, 12, 255},
	.success        = {82, 214, 152, 255},
	.error          = {232, 104, 92, 255},
	.default_accent = {48, 192, 204, 255},
};

constexpr Theme K_PINK{
	.window         = {15, 10, 14, 255},
	.chrome         = {21, 13, 19, 255},
	.chrome_seam    = {54, 26, 46, 255},
	.surface        = {25, 15, 23, 255},
	.popup          = {32, 18, 29, 255},
	.field          = {17, 10, 16, 255},
	.control        = {46, 24, 40, 255},
	.control_hover  = {64, 33, 56, 255},
	.border         = {80, 38, 68, 255},
	.separator      = {54, 27, 47, 255},
	.row_hover      = {40, 21, 35, 255},
	.row_selected   = {60, 30, 52, 255},
	.track          = {86, 44, 74, 255},
	.text           = {248, 228, 240, 255},
	.text_dim       = {196, 150, 178, 255},
	.text_faint     = {134, 94, 118, 255},
	.scroll_thumb   = {176, 98, 150, 190},
	.scrim          = {8, 0, 6, 160},
	.shadow         = {10, 0, 8, 255},
	.success        = {130, 222, 170, 255},
	.error          = {255, 92, 112, 255},
	.default_accent = {255, 62, 158, 255},
};

constexpr Theme K_BLOSSOM{
	.window         = {250, 236, 241, 255},
	.chrome         = {255, 244, 248, 255},
	.chrome_seam    = {242, 214, 226, 255},
	.surface        = {255, 250, 252, 255},
	.popup          = {255, 253, 254, 255},
	.field          = {253, 240, 245, 255},
	.control        = {249, 226, 236, 255},
	.control_hover  = {244, 212, 226, 255},
	.border         = {238, 200, 216, 255},
	.separator      = {246, 224, 233, 255},
	.row_hover      = {252, 238, 244, 255},
	.row_selected   = {248, 222, 234, 255},
	.track          = {232, 196, 212, 255},
	.text           = {88, 50, 74, 255},
	.text_dim       = {150, 106, 132, 255},
	.text_faint     = {178, 132, 158, 255},
	.scroll_thumb   = {226, 170, 196, 190},
	.scrim          = {120, 60, 90, 60},
	.shadow         = {150, 70, 110, 255},
	.success        = {82, 176, 132, 255},
	.error          = {224, 84, 110, 255},
	.default_accent = {238, 118, 168, 255},
};

constexpr Theme K_GRUVBOX{
	.window         = {29, 32, 33, 255},
	.chrome         = {40, 40, 40, 255},
	.chrome_seam    = {60, 56, 54, 255},
	.surface        = {40, 40, 40, 255},
	.popup          = {50, 48, 47, 255},
	.field          = {29, 32, 33, 255},
	.control        = {60, 56, 54, 255},
	.control_hover  = {80, 73, 69, 255},
	.border         = {80, 73, 69, 255},
	.separator      = {60, 56, 54, 255},
	.row_hover      = {50, 48, 47, 255},
	.row_selected   = {60, 56, 54, 255},
	.track          = {102, 92, 84, 255},
	.text           = {235, 219, 178, 255},
	.text_dim       = {168, 153, 132, 255},
	.text_faint     = {124, 111, 100, 255},
	.scroll_thumb   = {124, 111, 100, 200},
	.scrim          = {0, 0, 0, 150},
	.shadow         = {0, 0, 0, 255},
	.success        = {184, 187, 38, 255},
	.error          = {251, 73, 52, 255},
	.default_accent = {254, 128, 25, 255},
};

constexpr Theme K_CATPPUCCIN_MOCHA{
	.window         = {24, 24, 37, 255},
	.chrome         = {17, 17, 27, 255},
	.chrome_seam    = {49, 50, 68, 255},
	.surface        = {30, 30, 46, 255},
	.popup          = {36, 36, 54, 255},
	.field          = {24, 24, 37, 255},
	.control        = {49, 50, 68, 255},
	.control_hover  = {69, 71, 90, 255},
	.border         = {69, 71, 90, 255},
	.separator      = {49, 50, 68, 255},
	.row_hover      = {40, 40, 58, 255},
	.row_selected   = {49, 50, 68, 255},
	.track          = {88, 91, 112, 255},
	.text           = {205, 214, 244, 255},
	.text_dim       = {166, 173, 200, 255},
	.text_faint     = {108, 112, 134, 255},
	.scroll_thumb   = {127, 132, 156, 200},
	.scrim          = {17, 17, 27, 160},
	.shadow         = {10, 10, 18, 255},
	.success        = {166, 227, 161, 255},
	.error          = {243, 139, 168, 255},
	.default_accent = {203, 166, 247, 255},
};

constexpr Theme K_CATPPUCCIN_LATTE{
	.window         = {230, 233, 239, 255},
	.chrome         = {239, 241, 245, 255},
	.chrome_seam    = {204, 208, 218, 255},
	.surface        = {239, 241, 245, 255},
	.popup          = {245, 246, 249, 255},
	.field          = {230, 233, 239, 255},
	.control        = {220, 224, 232, 255},
	.control_hover  = {204, 208, 218, 255},
	.border         = {188, 192, 204, 255},
	.separator      = {220, 224, 232, 255},
	.row_hover      = {230, 233, 239, 255},
	.row_selected   = {220, 224, 232, 255},
	.track          = {172, 176, 190, 255},
	.text           = {76, 79, 105, 255},
	.text_dim       = {108, 111, 133, 255},
	.text_faint     = {140, 143, 161, 255},
	.scroll_thumb   = {156, 160, 176, 200},
	.scrim          = {76, 79, 105, 70},
	.shadow         = {76, 79, 105, 255},
	.success        = {64, 160, 43, 255},
	.error          = {210, 15, 57, 255},
	.default_accent = {136, 57, 239, 255},
};

constexpr Theme K_NORD{
	.window         = {46, 52, 64, 255},
	.chrome         = {39, 44, 54, 255},
	.chrome_seam    = {59, 66, 82, 255},
	.surface        = {52, 59, 73, 255},
	.popup          = {59, 66, 82, 255},
	.field          = {46, 52, 64, 255},
	.control        = {67, 76, 94, 255},
	.control_hover  = {76, 86, 106, 255},
	.border         = {76, 86, 106, 255},
	.separator      = {59, 66, 82, 255},
	.row_hover      = {59, 66, 82, 255},
	.row_selected   = {67, 76, 94, 255},
	.track          = {76, 86, 106, 255},
	.text           = {236, 239, 244, 255},
	.text_dim       = {170, 180, 198, 255},
	.text_faint     = {124, 135, 156, 255},
	.scroll_thumb   = {110, 122, 145, 200},
	.scrim          = {20, 24, 30, 150},
	.shadow         = {15, 18, 24, 255},
	.success        = {163, 190, 140, 255},
	.error          = {191, 97, 106, 255},
	.default_accent = {136, 192, 208, 255},
};

constexpr Theme K_DRACULA{
	.window         = {33, 34, 44, 255},
	.chrome         = {25, 26, 33, 255},
	.chrome_seam    = {68, 71, 90, 255},
	.surface        = {40, 42, 54, 255},
	.popup          = {46, 48, 62, 255},
	.field          = {33, 34, 44, 255},
	.control        = {68, 71, 90, 255},
	.control_hover  = {84, 88, 112, 255},
	.border         = {68, 71, 90, 255},
	.separator      = {52, 55, 70, 255},
	.row_hover      = {50, 52, 66, 255},
	.row_selected   = {68, 71, 90, 255},
	.track          = {84, 88, 112, 255},
	.text           = {248, 248, 242, 255},
	.text_dim       = {170, 175, 200, 255},
	.text_faint     = {98, 114, 164, 255},
	.scroll_thumb   = {98, 114, 164, 200},
	.scrim          = {12, 12, 18, 150},
	.shadow         = {10, 10, 15, 255},
	.success        = {80, 250, 123, 255},
	.error          = {255, 85, 85, 255},
	.default_accent = {189, 147, 249, 255},
};

constexpr Theme K_TOKYO_NIGHT{
	.window         = {26, 27, 38, 255},
	.chrome         = {22, 22, 30, 255},
	.chrome_seam    = {41, 46, 66, 255},
	.surface        = {31, 33, 46, 255},
	.popup          = {36, 38, 54, 255},
	.field          = {22, 22, 30, 255},
	.control        = {41, 46, 66, 255},
	.control_hover  = {52, 58, 84, 255},
	.border         = {52, 58, 84, 255},
	.separator      = {41, 46, 66, 255},
	.row_hover      = {36, 40, 58, 255},
	.row_selected   = {41, 46, 66, 255},
	.track          = {65, 72, 104, 255},
	.text           = {192, 202, 245, 255},
	.text_dim       = {140, 150, 190, 255},
	.text_faint     = {86, 95, 137, 255},
	.scroll_thumb   = {86, 95, 137, 200},
	.scrim          = {10, 10, 16, 150},
	.shadow         = {8, 8, 14, 255},
	.success        = {158, 206, 106, 255},
	.error          = {247, 118, 142, 255},
	.default_accent = {122, 162, 247, 255},
};

constexpr Theme K_ROSE_PINE{
	.window         = {25, 23, 36, 255},
	.chrome         = {21, 19, 31, 255},
	.chrome_seam    = {38, 35, 58, 255},
	.surface        = {31, 29, 46, 255},
	.popup          = {38, 35, 58, 255},
	.field          = {25, 23, 36, 255},
	.control        = {48, 45, 68, 255},
	.control_hover  = {64, 61, 82, 255},
	.border         = {64, 61, 82, 255},
	.separator      = {38, 35, 58, 255},
	.row_hover      = {36, 33, 52, 255},
	.row_selected   = {48, 45, 68, 255},
	.track          = {82, 79, 103, 255},
	.text           = {224, 222, 244, 255},
	.text_dim       = {144, 140, 170, 255},
	.text_faint     = {110, 106, 134, 255},
	.scroll_thumb   = {110, 106, 134, 200},
	.scrim          = {10, 8, 16, 150},
	.shadow         = {8, 6, 14, 255},
	.success        = {156, 207, 216, 255},
	.error          = {235, 111, 146, 255},
	.default_accent = {235, 188, 186, 255},
};

constexpr Theme K_PRESETS[]{
	K_DARK, K_LIGHT, K_FOREST, K_OCEAN, K_PINK, K_BLOSSOM, K_GRUVBOX, K_CATPPUCCIN_MOCHA, K_CATPPUCCIN_LATTE, K_NORD, K_DRACULA, K_TOKYO_NIGHT, K_ROSE_PINE,
};
static_assert(std::size(K_PRESETS) == K_THEME_COUNT);

constexpr usize K_COLORS_PER_THEME = sizeof(Theme) / sizeof(Color);
using ThemeColors                  = std::array<Color, K_COLORS_PER_THEME>;
static_assert(sizeof(ThemeColors) == sizeof(Theme), "a Theme must hold nothing but Colors");

struct ThemeFade {
	Theme     from;
	ThemeKind to;
	float     progress;
};

ThemeFade g_fade{K_DARK, ThemeKind::Dark, 1.0f};

[[nodiscard]] auto blend(u8 t_from, u8 t_to, float t_amount) -> u8
{
	return static_cast<u8>(std::lround(t_from + (static_cast<float>(t_to) - t_from) * t_amount));
}

[[nodiscard]] auto blend(Color t_from, Color t_to, float t_amount) -> Color
{
	return Color{blend(t_from.r, t_to.r, t_amount), blend(t_from.g, t_to.g, t_amount), blend(t_from.b, t_to.b, t_amount), blend(t_from.a, t_to.a, t_amount)};
}

[[nodiscard]] auto blend(const Theme& t_from, const Theme& t_to, float t_amount) -> Theme
{
	const auto  from = std::bit_cast<ThemeColors>(t_from);
	const auto  to   = std::bit_cast<ThemeColors>(t_to);
	ThemeColors blended{};

	for (usize i = 0; i < K_COLORS_PER_THEME; i += 1) {
		blended[i] = blend(from[i], to[i], t_amount);
	}

	return std::bit_cast<Theme>(blended);
}
}

Theme g_theme = K_DARK;

[[nodiscard]] auto theme_preset(ThemeKind t_kind) -> const Theme&
{
	return K_PRESETS[static_cast<u32>(t_kind)];
}

[[nodiscard]] auto hovered(Color t_base) -> Color
{
	constexpr float LIGHT_SURFACE = 0.3f;
	constexpr float DARKEN        = 0.06f;
	constexpr float LIGHTEN       = 0.08f;

	return luminance(t_base) > LIGHT_SURFACE ? mix(t_base, Color{0, 0, 0, 255}, DARKEN) : mix(t_base, Color{255, 255, 255, 255}, LIGHTEN);
}

auto apply_theme(ThemeKind t_kind) -> void
{
	g_theme = theme_preset(t_kind);
	g_fade  = ThemeFade{g_theme, t_kind, 1.0f};
}

auto fade_to_theme(ThemeKind t_kind) -> void
{
	if (t_kind == g_fade.to) return;

	g_fade = ThemeFade{g_theme, t_kind, 0.0f};
}

auto update_theme(float t_delta_seconds) -> void
{
	if (g_fade.progress >= 1.0f) return;

	g_fade.progress = animation::ease_toward(g_fade.progress, 1.0f, K_FADE_RATE, t_delta_seconds);
	g_theme         = blend(g_fade.from, theme_preset(g_fade.to), g_fade.progress);
}
