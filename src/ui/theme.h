#pragma once

#include "core/settings.h"
#include "core/types.h"

struct Theme {
	Color window;
	Color chrome;
	Color chrome_seam;
	Color surface;
	Color popup;
	Color field;
	Color control;
	Color control_hover;
	Color border;
	Color separator;
	Color row_hover;
	Color row_selected;
	Color track;
	Color text;
	Color text_dim;
	Color text_faint;
	Color scroll_thumb;
	Color scrim;
	Color shadow;
	Color success;
	Color error;
	Color default_accent;
};

const Theme &theme();
const Theme &theme_preset(ThemeKind t_kind);
Color hovered(Color t_base);

void apply_theme(ThemeKind t_kind);
void fade_to_theme(ThemeKind t_kind);
void update_theme(float t_delta_seconds);
