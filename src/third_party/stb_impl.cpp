// Compiles the vendored stb single-header implementations exactly once. Nothing in f4
// calls into these yet: stb_image is for banner/icon loading (see the u/v fields
// already reserved on gfx::Vertex_2D), stb_truetype + stb_rect_pack are for a glyph
// atlas once text rendering lands. This file exists so the include path and build
// wiring are proven out ahead of that work, not to implement it now.

#pragma warning(push)
#pragma warning(disable : 4996 4244 4245 4457) // third-party code, not held to our warning level

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#pragma warning(pop)
