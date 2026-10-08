#pragma once

constexpr const char* K_GLSL_VERSION = "#version 330 core\n";

constexpr const char* K_VERTEX_SHADER_SOURCE = R"(
	layout(std140) uniform ViewportConstants {
		vec2 size;
		vec2 padding;
	} viewport;

	layout(location = 0) in vec2 in_position;
	layout(location = 1) in vec2 in_uv;
	layout(location = 2) in vec4 in_color;

	out vec2 pixel_uv;
	out vec4 pixel_color;

	void main()
	{
		vec2 ndc = vec2(in_position.x / viewport.size.x * 2.0 - 1.0,
						1.0 - in_position.y / viewport.size.y * 2.0);
		gl_Position = vec4(ndc, 0.0, 1.0);
		pixel_uv = in_uv;
		pixel_color = in_color;
	}
)";

// A triangle that covers the target, for the blur passes. Its uv runs bottom-up like the textures it reads.
constexpr const char* K_FULLSCREEN_VERTEX_SHADER_SOURCE = R"(
	out vec2 pixel_uv;
	out vec4 pixel_color;

	void main()
	{
		vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
		gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
		pixel_uv = uv;
		pixel_color = vec4(1.0);
	}
)";

constexpr const char* K_FRAGMENT_SHADER_SOURCE = R"(
	layout(origin_upper_left) in vec4 gl_FragCoord;

	in vec2 pixel_uv;
	in vec4 pixel_color;

	out vec4 out_color;

	uniform sampler2D image;

	float saturate(float x)
	{
		return clamp(x, 0.0, 1.0);
	}

	vec3 saturate(vec3 x)
	{
		return clamp(x, 0.0, 1.0);
	}

	vec4 ps_solid()
	{
		return pixel_color;
	}

	vec4 ps_textured()
	{
		return texture(image, pixel_uv) * pixel_color;
	}

	layout(std140) uniform BannerGlowConstants {
		float time_seconds;
		float quad_width;
		float quad_height;
		float corner_radius;
		float ring_width;
	} banner;

	float rounded_box_distance(vec2 p, vec2 half_size, float radius)
	{
		vec2 q = abs(p) - half_size + radius;
		return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
	}

	float wrapped_angle_between(float a, float b)
	{
		float d = a - b;
		return atan(sin(d), cos(d));
	}

	vec4 ps_banner_glow()
	{
		const float pi = 3.14159265;
		const float comet_width = 0.6;

		vec2 local = (pixel_uv - 0.5) * vec2(banner.quad_width, banner.quad_height);
		vec2 card_half_size = vec2(banner.quad_width, banner.quad_height) * 0.5 - banner.ring_width;
		float ring = max(banner.ring_width, 0.001);

		float distance_out = saturate(rounded_box_distance(local, card_half_size, banner.corner_radius) / ring);
		float band = pow(saturate(1.0 - distance_out), 1.8);
		float ambient = band * 0.32;

		vec2 normalized = local / max(card_half_size, 1.0);
		float angle = atan(normalized.y, normalized.x);
		float rotation = banner.time_seconds * 1.1;
		float comet_a = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation)));
		float comet_b = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation + pi)));

		float glow = saturate(ambient + band * max(comet_a, comet_b) * 0.85);
		glow *= 1.0 - smoothstep(0.85, 1.0, distance_out);
		return vec4(pixel_color.rgb * glow, pixel_color.a * glow);
	}

	layout(std140) uniform ShadowConstants {
		float quad_width;
		float quad_height;
		float corner_radius;
		float blur;
	} shadow;

	float approximate_erf(float x)
	{
		float magnitude = abs(x);
		float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * magnitude * magnitude) * magnitude) * magnitude;
		t *= t;
		return sign(x) * (1.0 - 1.0 / (t * t));
	}

	vec4 ps_shadow()
	{
		vec2 quad_size = vec2(shadow.quad_width, shadow.quad_height);
		vec2 local = (pixel_uv - 0.5) * quad_size;
		float edge_distance = rounded_box_distance(local, quad_size * 0.5 - shadow.blur, shadow.corner_radius);
		float sigma = max(shadow.blur * 0.5, 0.001);
		float coverage = 0.5 - 0.5 * approximate_erf(edge_distance / (sigma * 1.41421356));
		return vec4(pixel_color.rgb, pixel_color.a * coverage);
	}

	vec3 hsv_to_rgb(float h, float s, float v)
	{
		vec3 rgb = saturate(abs(mod(h / 60.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
		return v * mix(vec3(1.0), rgb, s);
	}

	vec4 ps_color_picker()
	{
		float hue = pixel_color.r * 360.0;
		return vec4(hsv_to_rgb(hue, pixel_uv.x, 1.0 - pixel_uv.y), 1.0);
	}

	layout(std140) uniform OutlineCountdownConstants {
		float quad_width;
		float quad_height;
		float half_width;
		float half_height;
		float radius;
		float thickness;
		float glow_radius;
		float lit_length;
		vec2 end_point;
	} outline;

	float outline_path_position(vec2 clamped, vec2 direction, vec2 core, float radius)
	{
		const float pi = 3.14159265;
		const float half_pi = 1.57079633;
		float quarter = half_pi * radius;
		float angle = atan(direction.y, direction.x);
		float position = 3.0 * core.x + 3.0 * quarter + 4.0 * core.y + radius * (angle + pi);

		if (direction.x == 0.0 && direction.y < 0.0) {
			position = clamped.x >= 0.0 ? clamped.x : 4.0 * (core.x + core.y + quarter) + clamped.x;
		} else if (direction.x > 0.0 && direction.y < 0.0) {
			position = core.x + radius * (angle + half_pi);
		} else if (direction.x > 0.0 && direction.y == 0.0) {
			position = core.x + quarter + clamped.y + core.y;
		} else if (direction.x > 0.0) {
			position = core.x + quarter + 2.0 * core.y + radius * angle;
		} else if (direction.x == 0.0) {
			position = core.x + 2.0 * quarter + 2.0 * core.y + core.x - clamped.x;
		} else if (direction.y > 0.0) {
			position = 3.0 * core.x + 2.0 * quarter + 2.0 * core.y + radius * (angle - half_pi);
		} else if (direction.y == 0.0) {
			position = 3.0 * core.x + 3.0 * quarter + 2.0 * core.y + core.y - clamped.y;
		}

		return position;
	}

	vec4 ps_outline_countdown()
	{
		vec2 local = (pixel_uv - 0.5) * vec2(outline.quad_width, outline.quad_height);
		float radius = outline.radius;
		vec2 core = max(vec2(outline.half_width, outline.half_height) - radius, 0.0);

		vec2 clamped = clamp(local, -core, core);
		vec2 offset = local - clamped;
		vec2 direction;
		float path_distance;

		if (dot(offset, offset) > 0.000001) {
			direction = normalize(offset);
			path_distance = abs(length(offset) - radius);
		} else {
			vec2 gap = core - abs(local);
			vec2 side = vec2(local.x < 0.0 ? -1.0 : 1.0, local.y < 0.0 ? -1.0 : 1.0);
			direction = gap.x < gap.y ? vec2(side.x, 0.0) : vec2(0.0, side.y);
			path_distance = min(gap.x, gap.y) + radius;
		}

		float position = outline_path_position(clamped, direction, core, radius);
		float start_distance = length(local - vec2(0.0, -(core.y + radius)));
		float end_distance = length(local - outline.end_point);

		float line_distance = path_distance;
		float progress = saturate(position / max(outline.lit_length, 0.0001));

		if (position > outline.lit_length) {
			line_distance = min(start_distance, end_distance);
			progress = smoothstep(-1.0, 1.0, start_distance - end_distance);
		}

		float half_thickness = outline.thickness * 0.5;
		float footprint = max(fwidth(line_distance), 0.0001) * 1.25;
		float coverage = saturate((half_thickness - line_distance) / footprint + 0.5);
		float line_alpha = mix(coverage, sqrt(coverage), 0.35);

		float emphasis = progress * progress;
		vec3 base = pixel_color.rgb;
		vec3 line_rgb = mix(base * 0.85, mix(base, vec3(1.0, 0.62, 0.7), 0.3), emphasis);

		float glow_distance = max(line_distance - half_thickness, 0.0);
		float glow_falloff = exp(-(glow_distance * glow_distance) / (2.0 * outline.glow_radius * outline.glow_radius));
		float glow = (0.12 + 0.12 * emphasis) * glow_falloff * (1.0 - line_alpha);

		float alpha = saturate(line_alpha + glow);
		vec3 rgb = alpha > 0.0001 ? (line_rgb * line_alpha + base * glow) / alpha : base;

		return vec4(saturate(rgb), alpha * pixel_color.a);
	}

	layout(std140) uniform BackdropConstants {
		vec2 target_size;
		float intensity;
		float style;
		float pixel_scale;
		float light;
		float grain;
	} backdrop;

	const float backdrop_pattern_strength[9] = float[9](0.0, 0.2, 0.035, 0.05, 0.09, 0.08, 0.5, 0.045, 0.04);
	const float backdrop_swatch_strength[9] = float[9](0.0, 0.55, 0.2, 0.2, 0.55, 0.35, 1.1, 0.12, 0.15);
	const float backdrop_swatch_scale[9] = float[9](1.0, 0.5, 0.4, 0.7, 0.5, 0.35, 0.4, 1.0, 0.6);
	const float backdrop_swatch_code = 8192.0;
	const int backdrop_bias_cells = 4096;

	float backdrop_noise(vec2 pixel)
	{
		vec3 p = fract(pixel.xyx * 0.1031);
		p += dot(p, p.yzx + 33.33);
		return fract((p.x + p.y) * p.z);
	}

	float backdrop_hash(vec2 p)
	{
		uvec2 q = uvec2(ivec2(p));
		uint h = q.x * 374761393u + q.y * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return float(h) / 4294967296.0;
	}

	float backdrop_value_noise(vec2 p)
	{
		vec2 i = floor(p);
		vec2 f = p - i;
		vec2 s = f * f * (3.0 - 2.0 * f);
		float a = backdrop_hash(i);
		float b = backdrop_hash(i + vec2(1.0, 0.0));
		float c = backdrop_hash(i + vec2(0.0, 1.0));
		float d = backdrop_hash(i + vec2(1.0, 1.0));
		return a + (b - a) * s.x + (c - a) * s.y + (a - b - c + d) * s.x * s.y;
	}

	int backdrop_wrap(int a, int b)
	{
		return int(uint(a + b * backdrop_bias_cells) % uint(b));
	}

	int backdrop_floor_div(int a, int b)
	{
		return int(uint(a + b * backdrop_bias_cells) / uint(b)) - backdrop_bias_cells;
	}

	int backdrop_spacing(float logical, float scale)
	{
		return max(int(roundEven(logical * scale)), 2);
	}

	float backdrop_pattern(vec2 pixel, vec2 origin, float scale, int style)
	{
		int stroke = max(int(roundEven(scale)), 1);
		ivec2 p = ivec2(floor(pixel)) - ivec2(origin);

		int dot_spacing = backdrop_spacing(24.0, scale);
		float dots = backdrop_wrap(p.x, dot_spacing) < stroke && backdrop_wrap(p.y, dot_spacing) < stroke ? 1.0 : 0.0;

		int grid_spacing = backdrop_spacing(40.0, scale);
		float grid = backdrop_wrap(p.x, grid_spacing) == 0 || backdrop_wrap(p.y, grid_spacing) == 0 ? 1.0 : 0.0;

		float lines = backdrop_wrap(p.x + p.y, backdrop_spacing(10.0, scale)) < stroke ? 1.0 : 0.0;

		int polka_spacing = backdrop_spacing(28.0, scale);
		int polka_row = backdrop_floor_div(p.y, polka_spacing);
		int polka_offset = backdrop_wrap(polka_row, 2) == 1 ? polka_spacing >> 1 : 0;
		int polka_column = backdrop_floor_div(p.x - polka_offset, polka_spacing);
		vec2 polka_center = vec2(float(polka_column * polka_spacing + polka_offset), float(polka_row * polka_spacing)) +
							float(polka_spacing) * 0.5;
		float polka = saturate(2.0 * scale + 0.5 - length(vec2(p) - polka_center));

		vec2 terrain = vec2(p) / (170.0 * scale);
		float height = (backdrop_value_noise(terrain) * 0.65 +
						backdrop_value_noise(terrain * 2.1 + vec2(5.2, 1.3)) * 0.35) * 8.0;
		float contour = min(fract(height), 1.0 - fract(height)) / max(length(vec2(dFdx(height), dFdy(height))), 0.00001);
		float topography = saturate(1.0 - contour / 0.9);

		int star_spacing = backdrop_spacing(14.0, scale);
		ivec2 star_cell = ivec2(backdrop_floor_div(p.x, star_spacing), backdrop_floor_div(p.y, star_spacing));
		vec2 cell = vec2(star_cell);
		vec2 star_jitter = vec2(backdrop_hash(cell + vec2(9.0, 0.0)), backdrop_hash(cell + vec2(0.0, 9.0)));
		ivec2 star = star_cell * star_spacing + 1 + ivec2(star_jitter * float(max(star_spacing - 3, 1)));
		float star_seed = backdrop_hash(cell + vec2(5.0, 3.0));
		float star_distance = length(vec2(p - star));
		float star_sigma = max(0.5 * scale, 0.35);
		float star_shape = star_seed > 0.9 ? exp(-star_distance * star_distance / (2.0 * star_sigma * star_sigma))
										   : (star_distance < 0.5 ? 1.0 : 0.0);
		float star_cluster = backdrop_value_noise(cell / 7.0 + vec2(3.1, 7.7));
		bool has_star = backdrop_hash(cell) < 0.07 + 0.12 * star_cluster * star_cluster;
		float stars = has_star ? star_shape * (0.3 + 0.7 * star_seed * star_seed) : 0.0;

		int dust_spacing = backdrop_spacing(9.0, scale);
		ivec2 dust_cell = ivec2(backdrop_floor_div(p.x, dust_spacing), backdrop_floor_div(p.y, dust_spacing));
		vec2 dust_key = vec2(dust_cell) + vec2(101.0, 57.0);
		ivec2 dust = dust_cell * dust_spacing +
					 ivec2(vec2(backdrop_hash(dust_key + vec2(9.0, 0.0)), backdrop_hash(dust_key + vec2(0.0, 9.0))) *
						   float(dust_spacing));
		float dust_level = p == dust && backdrop_hash(dust_key) < 0.2
							   ? 0.12 + 0.12 * backdrop_hash(dust_key + vec2(5.0, 3.0))
							   : 0.0;
		float starfield = max(stars, dust_level);

		float scanlines = backdrop_wrap(p.y, backdrop_spacing(3.0, scale)) < stroke ? 1.0 : 0.0;

		int hatch_spacing = backdrop_spacing(14.0, scale);
		float crosshatch =
			backdrop_wrap(p.x + p.y, hatch_spacing) < stroke || backdrop_wrap(p.x - p.y, hatch_spacing) < stroke ? 1.0
																												 : 0.0;

		float pattern = 0.0;
		pattern = style == 1 ? dots : pattern;
		pattern = style == 2 ? grid : pattern;
		pattern = style == 3 ? lines : pattern;
		pattern = style == 4 ? polka : pattern;
		pattern = style == 5 ? topography : pattern;
		pattern = style == 6 ? starfield : pattern;
		pattern = style == 7 ? scanlines : pattern;
		pattern = style == 8 ? crosshatch : pattern;

		return pattern;
	}

	vec4 backdrop_color(bool with_pattern)
	{
		bool swatch = pixel_uv.x > backdrop_swatch_code * 0.5;
		int swatch_style = int(pixel_uv.x / backdrop_swatch_code) - 1;
		int style = clamp(swatch ? swatch_style : int(backdrop.style + 0.5), 0, 8);
		vec2 pixel = gl_FragCoord.xy;
		vec2 size = backdrop.target_size;
		vec2 origin = swatch ? roundEven(vec2(pixel_uv.x - float(swatch_style + 1) * backdrop_swatch_code, pixel_uv.y))
							 : floor(size * 0.5);
		float strength = swatch ? backdrop_swatch_strength[style] : backdrop_pattern_strength[style] * backdrop.intensity;

		float light = saturate(1.0 - (pixel.y + size.y * 0.2) / (size.y * 1.3));
		float scale = max(backdrop.pixel_scale, 0.5) * (swatch ? backdrop_swatch_scale[style] : 1.0);
		float pattern = with_pattern ? backdrop_pattern(pixel, origin, scale, style) : 0.0;
		vec3 ink = vec3(dot(pixel_color.rgb, vec3(0.299, 0.587, 0.114)) > 0.5 ? 0.0 : 1.0);

		vec3 rgb = pixel_color.rgb;
		rgb = mix(rgb, vec3(1.0), light * 0.07 * (swatch ? 0.0 : backdrop.light));
		rgb = mix(rgb, ink, pattern * strength);
		rgb += (backdrop_noise(pixel) - 0.5) * (10.5 / 255.0) * (swatch ? 0.0 : backdrop.grain);

		return vec4(saturate(rgb), pixel_color.a);
	}

	vec4 ps_backdrop()
	{
		return backdrop_color(true);
	}

	vec4 ps_backdrop_plain()
	{
		return backdrop_color(false);
	}

	layout(std140) uniform BlurConstants {
		vec2 target_size;
		vec2 texel_step;
	} blur;

	// Reads the blurred level as a cubic B-spline in four bilinear taps. Plain bilinear stretched over many pixels shows the level's
	// texels as blocks; the spline is smooth across them.
	vec3 sample_blurred(vec2 uv)
	{
		vec2 coord = uv / blur.texel_step - 0.5;
		vec2 base = floor(coord);
		vec2 f = coord - base;
		vec2 f2 = f * f;
		vec2 f3 = f2 * f;
		vec2 w0 = (1.0 - 3.0 * f + 3.0 * f2 - f3) / 6.0;
		vec2 w1 = (4.0 - 6.0 * f2 + 3.0 * f3) / 6.0;
		vec2 w3 = f3 / 6.0;
		vec2 g0 = w0 + w1;
		vec2 g1 = 1.0 - g0;
		vec2 t0 = (base - 0.5 + w1 / g0) * blur.texel_step;
		vec2 t1 = (base + 1.5 + w3 / g1) * blur.texel_step;

		return (texture(image, t0).rgb * g0.x + texture(image, vec2(t1.x, t0.y)).rgb * g1.x) * g0.y +
		       (texture(image, vec2(t0.x, t1.y)).rgb * g0.x + texture(image, t1).rgb * g1.x) * g1.y;
	}

	vec4 ps_backdrop_blur()
	{
		vec2 uv = vec2(gl_FragCoord.x, blur.target_size.y - gl_FragCoord.y) / blur.target_size;
		vec3 rgb = sample_blurred(uv);
		float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
		rgb = mix(vec3(luma), rgb, 1.15) + (backdrop_noise(gl_FragCoord.xy) - 0.5) * (4.0 / 255.0);
		return vec4(saturate(rgb), pixel_color.a);
	}

	// Halves its source through five taps, a small tent rather than a box, so fine detail does not alias into the level below.
	vec4 ps_downsample()
	{
		vec2 diagonal = vec2(blur.texel_step.x, -blur.texel_step.y);
		vec4 sum = texture(image, pixel_uv) * 4.0;
		sum += texture(image, pixel_uv - blur.texel_step) + texture(image, pixel_uv + blur.texel_step);
		sum += texture(image, pixel_uv - diagonal) + texture(image, pixel_uv + diagonal);
		return sum * 0.125;
	}

	vec4 ps_blur()
	{
		const float offsets[4] = float[4](0.0, 1.411764706, 3.294117647, 5.176470588);
		const float weights[4] = float[4](0.196482550, 0.296906965, 0.094470398, 0.010381362);

		vec4 sum = texture(image, pixel_uv) * weights[0];
		for (int i = 1; i < 4; i++) {
			sum += texture(image, pixel_uv + blur.texel_step * offsets[i]) * weights[i];
			sum += texture(image, pixel_uv - blur.texel_step * offsets[i]) * weights[i];
		}

		return sum;
	}

	void main()
	{
		out_color = PS_MAIN();
	}
)";
