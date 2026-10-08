#pragma once

constexpr const char* K_SHADER_SOURCE = R"(
	#include <metal_stdlib>
	using namespace metal;

	struct ViewportConstants {
		float2 size;
		float2 padding;
	};

	struct VertexInput {
		float2 position [[attribute(0)]];
		float2 uv [[attribute(1)]];
		float4 color [[attribute(2)]];
	};

	struct PixelInput {
		float4 position [[position]];
		float2 uv;
		float4 color;
	};

	vertex PixelInput vs_main(VertexInput input [[stage_in]], constant ViewportConstants& viewport [[buffer(1)]])
	{
		PixelInput output;
		float2 ndc = float2(input.position.x / viewport.size.x * 2.0 - 1.0,
							1.0 - input.position.y / viewport.size.y * 2.0);
		output.position = float4(ndc, 0.0, 1.0);
		output.uv = input.uv;
		output.color = input.color;
		return output;
	}

	fragment float4 ps_solid(PixelInput input [[stage_in]])
	{
		return input.color;
	}

	fragment float4 ps_textured(PixelInput input [[stage_in]], texture2d<float> image [[texture(0)]], sampler image_sampler [[sampler(0)]])
	{
		return image.sample(image_sampler, input.uv) * input.color;
	}

	struct BannerGlowConstants {
		float time_seconds;
		float quad_width;
		float quad_height;
		float corner_radius;
		float ring_width;
	};

	float rounded_box_distance(float2 p, float2 half_size, float radius)
	{
		float2 q = abs(p) - half_size + radius;
		return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
	}

	float wrapped_angle_between(float a, float b)
	{
		float d = a - b;
		return atan2(sin(d), cos(d));
	}

	fragment float4 ps_banner_glow(PixelInput input [[stage_in]], constant BannerGlowConstants& banner [[buffer(1)]])
	{
		const float pi = 3.14159265;
		const float comet_width = 0.6;

		float2 local = (input.uv - 0.5) * float2(banner.quad_width, banner.quad_height);
		float2 card_half_size = float2(banner.quad_width, banner.quad_height) * 0.5 - banner.ring_width;
		float ring = max(banner.ring_width, 0.001);

		float distance_out = saturate(rounded_box_distance(local, card_half_size, banner.corner_radius) / ring);
		float band = pow(saturate(1.0 - distance_out), 1.8);
		float ambient = band * 0.32;

		float2 normalized = local / max(card_half_size, 1.0);
		float angle = atan2(normalized.y, normalized.x);
		float rotation = banner.time_seconds * 1.1;
		float comet_a = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation)));
		float comet_b = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation + pi)));

		float glow = saturate(ambient + band * max(comet_a, comet_b) * 0.85);
		glow *= 1.0 - smoothstep(0.85, 1.0, distance_out);
		return float4(input.color.rgb * glow, input.color.a * glow);
	}

	struct ShadowConstants {
		float quad_width;
		float quad_height;
		float corner_radius;
		float blur;
	};

	float approximate_erf(float x)
	{
		float magnitude = abs(x);
		float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * magnitude * magnitude) * magnitude) * magnitude;
		t *= t;
		return sign(x) * (1.0 - 1.0 / (t * t));
	}

	fragment float4 ps_shadow(PixelInput input [[stage_in]], constant ShadowConstants& shadow [[buffer(1)]])
	{
		float2 quad_size = float2(shadow.quad_width, shadow.quad_height);
		float2 local = (input.uv - 0.5) * quad_size;
		float edge_distance = rounded_box_distance(local, quad_size * 0.5 - shadow.blur, shadow.corner_radius);
		float sigma = max(shadow.blur * 0.5, 0.001);
		float coverage = 0.5 - 0.5 * approximate_erf(edge_distance / (sigma * 1.41421356));
		return float4(input.color.rgb, input.color.a * coverage);
	}

	float3 hsv_to_rgb(float h, float s, float v)
	{
		float3 rgb = saturate(abs(fmod(h / 60.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
		return v * mix(float3(1.0), rgb, s);
	}

	fragment float4 ps_color_picker(PixelInput input [[stage_in]])
	{
		float hue = input.color.r * 360.0;
		return float4(hsv_to_rgb(hue, input.uv.x, 1.0 - input.uv.y), 1.0);
	}

	struct OutlineCountdownConstants {
		float quad_width;
		float quad_height;
		float half_width;
		float half_height;
		float radius;
		float thickness;
		float glow_radius;
		float lit_length;
		float2 end_point;
	};

	float outline_path_position(float2 clamped, float2 direction, float2 core, float radius)
	{
		const float pi = 3.14159265;
		const float half_pi = 1.57079633;
		float quarter = half_pi * radius;
		float angle = atan2(direction.y, direction.x);
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

	fragment float4 ps_outline_countdown(PixelInput input [[stage_in]], constant OutlineCountdownConstants& outline [[buffer(1)]])
	{
		float2 local = (input.uv - 0.5) * float2(outline.quad_width, outline.quad_height);
		float radius = outline.radius;
		float2 core = max(float2(outline.half_width, outline.half_height) - radius, 0.0);

		float2 clamped = clamp(local, -core, core);
		float2 offset = local - clamped;
		float2 direction;
		float path_distance;

		if (dot(offset, offset) > 0.000001) {
			direction = normalize(offset);
			path_distance = abs(length(offset) - radius);
		} else {
			float2 gap = core - abs(local);
			float2 side = float2(local.x < 0.0 ? -1.0 : 1.0, local.y < 0.0 ? -1.0 : 1.0);
			direction = gap.x < gap.y ? float2(side.x, 0.0) : float2(0.0, side.y);
			path_distance = min(gap.x, gap.y) + radius;
		}

		float position = outline_path_position(clamped, direction, core, radius);
		float start_distance = length(local - float2(0.0, -(core.y + radius)));
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
		float3 base = input.color.rgb;
		float3 line_rgb = mix(base * 0.85, mix(base, float3(1.0, 0.62, 0.7), 0.3), emphasis);

		float glow_distance = max(line_distance - half_thickness, 0.0);
		float glow_falloff = exp(-(glow_distance * glow_distance) / (2.0 * outline.glow_radius * outline.glow_radius));
		float glow = (0.12 + 0.12 * emphasis) * glow_falloff * (1.0 - line_alpha);

		float alpha = saturate(line_alpha + glow);
		float3 rgb = alpha > 0.0001 ? (line_rgb * line_alpha + base * glow) / alpha : base;

		return float4(saturate(rgb), alpha * input.color.a);
	}

	struct BackdropConstants {
		float2 target_size;
		float intensity;
		float style;
		float pixel_scale;
		float light;
		float grain;
	};

	constant float backdrop_pattern_strength[9] = {0.0, 0.2, 0.035, 0.05, 0.09, 0.08, 0.5, 0.045, 0.04};
	constant float backdrop_swatch_strength[9] = {0.0, 0.55, 0.2, 0.2, 0.55, 0.35, 1.1, 0.12, 0.15};
	constant float backdrop_swatch_scale[9] = {1.0, 0.5, 0.4, 0.7, 0.5, 0.35, 0.4, 1.0, 0.6};
	constant float backdrop_swatch_code = 8192.0;
	constant int backdrop_bias_cells = 4096;

	float backdrop_noise(float2 pixel)
	{
		float3 p = fract(pixel.xyx * 0.1031);
		p += dot(p, p.yzx + 33.33);
		return fract((p.x + p.y) * p.z);
	}

	float backdrop_hash(float2 p)
	{
		uint2 q = as_type<uint2>(int2(p));
		uint h = q.x * 374761393u + q.y * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return float(h) / 4294967296.0;
	}

	float backdrop_value_noise(float2 p)
	{
		float2 i = floor(p);
		float2 f = p - i;
		float2 s = f * f * (3.0 - 2.0 * f);
		float a = backdrop_hash(i);
		float b = backdrop_hash(i + float2(1.0, 0.0));
		float c = backdrop_hash(i + float2(0.0, 1.0));
		float d = backdrop_hash(i + float2(1.0, 1.0));
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
		return max(int(rint(logical * scale)), 2);
	}

	float backdrop_pattern(float2 pixel, float2 origin, float scale, int style)
	{
		int stroke = max(int(rint(scale)), 1);
		int2 p = int2(floor(pixel)) - int2(origin);

		int dot_spacing = backdrop_spacing(24.0, scale);
		float dots = backdrop_wrap(p.x, dot_spacing) < stroke && backdrop_wrap(p.y, dot_spacing) < stroke ? 1.0 : 0.0;

		int grid_spacing = backdrop_spacing(40.0, scale);
		float grid = backdrop_wrap(p.x, grid_spacing) == 0 || backdrop_wrap(p.y, grid_spacing) == 0 ? 1.0 : 0.0;

		float lines = backdrop_wrap(p.x + p.y, backdrop_spacing(10.0, scale)) < stroke ? 1.0 : 0.0;

		int polka_spacing = backdrop_spacing(28.0, scale);
		int polka_row = backdrop_floor_div(p.y, polka_spacing);
		int polka_offset = backdrop_wrap(polka_row, 2) == 1 ? polka_spacing >> 1 : 0;
		int polka_column = backdrop_floor_div(p.x - polka_offset, polka_spacing);
		float2 polka_center = float2(float(polka_column * polka_spacing + polka_offset), float(polka_row * polka_spacing)) +
							  float(polka_spacing) * 0.5;
		float polka = saturate(2.0 * scale + 0.5 - length(float2(p) - polka_center));

		float2 terrain = float2(p) / (170.0 * scale);
		float height = (backdrop_value_noise(terrain) * 0.65 +
						backdrop_value_noise(terrain * 2.1 + float2(5.2, 1.3)) * 0.35) * 8.0;
		float contour = min(fract(height), 1.0 - fract(height)) / max(length(float2(dfdx(height), dfdy(height))), 0.00001);
		float topography = saturate(1.0 - contour / 0.9);

		int star_spacing = backdrop_spacing(14.0, scale);
		int2 star_cell = int2(backdrop_floor_div(p.x, star_spacing), backdrop_floor_div(p.y, star_spacing));
		float2 cell = float2(star_cell);
		float2 star_jitter = float2(backdrop_hash(cell + float2(9.0, 0.0)), backdrop_hash(cell + float2(0.0, 9.0)));
		int2 star = star_cell * star_spacing + 1 + int2(star_jitter * float(max(star_spacing - 3, 1)));
		float star_seed = backdrop_hash(cell + float2(5.0, 3.0));
		float star_distance = length(float2(p - star));
		float star_sigma = max(0.5 * scale, 0.35);
		float star_shape = star_seed > 0.9 ? exp(-star_distance * star_distance / (2.0 * star_sigma * star_sigma))
										   : (star_distance < 0.5 ? 1.0 : 0.0);
		float star_cluster = backdrop_value_noise(cell / 7.0 + float2(3.1, 7.7));
		bool has_star = backdrop_hash(cell) < 0.07 + 0.12 * star_cluster * star_cluster;
		float stars = has_star ? star_shape * (0.3 + 0.7 * star_seed * star_seed) : 0.0;

		int dust_spacing = backdrop_spacing(9.0, scale);
		int2 dust_cell = int2(backdrop_floor_div(p.x, dust_spacing), backdrop_floor_div(p.y, dust_spacing));
		float2 dust_key = float2(dust_cell) + float2(101.0, 57.0);
		int2 dust = dust_cell * dust_spacing +
					int2(float2(backdrop_hash(dust_key + float2(9.0, 0.0)), backdrop_hash(dust_key + float2(0.0, 9.0))) *
						 float(dust_spacing));
		float dust_level = all(p == dust) && backdrop_hash(dust_key) < 0.2
							   ? 0.12 + 0.12 * backdrop_hash(dust_key + float2(5.0, 3.0))
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

	float4 backdrop_color(PixelInput input, constant BackdropConstants& backdrop, bool with_pattern)
	{
		bool swatch = input.uv.x > backdrop_swatch_code * 0.5;
		int swatch_style = int(input.uv.x / backdrop_swatch_code) - 1;
		int style = clamp(swatch ? swatch_style : int(backdrop.style + 0.5), 0, 8);
		float2 pixel = input.position.xy;
		float2 size = backdrop.target_size;
		float2 origin = swatch ? rint(float2(input.uv.x - float(swatch_style + 1) * backdrop_swatch_code, input.uv.y))
							   : floor(size * 0.5);
		float strength = swatch ? backdrop_swatch_strength[style] : backdrop_pattern_strength[style] * backdrop.intensity;

		float light = saturate(1.0 - (pixel.y + size.y * 0.2) / (size.y * 1.3));
		float scale = max(backdrop.pixel_scale, 0.5) * (swatch ? backdrop_swatch_scale[style] : 1.0);
		float pattern = with_pattern ? backdrop_pattern(pixel, origin, scale, style) : 0.0;
		float3 ink = float3(dot(input.color.rgb, float3(0.299, 0.587, 0.114)) > 0.5 ? 0.0 : 1.0);

		float3 rgb = input.color.rgb;
		rgb = mix(rgb, float3(1.0), light * 0.07 * (swatch ? 0.0 : backdrop.light));
		rgb = mix(rgb, ink, pattern * strength);
		rgb += (backdrop_noise(pixel) - 0.5) * (10.5 / 255.0) * (swatch ? 0.0 : backdrop.grain);

		return float4(saturate(rgb), input.color.a);
	}

	fragment float4 ps_backdrop(PixelInput input [[stage_in]], constant BackdropConstants& backdrop [[buffer(1)]])
	{
		return backdrop_color(input, backdrop, true);
	}

	fragment float4 ps_backdrop_plain(PixelInput input [[stage_in]], constant BackdropConstants& backdrop [[buffer(1)]])
	{
		return backdrop_color(input, backdrop, false);
	}

	struct BlurConstants {
		float2 target_size;
		float2 texel_step;
	};

	// Reads the blurred level as a cubic B-spline in four bilinear taps. Plain bilinear stretched over many pixels shows the level's
	// texels as blocks; the spline is smooth across them.
	float3 sample_blurred(texture2d<float> image, sampler image_sampler, float2 uv, float2 texel)
	{
		float2 coord = uv / texel - 0.5;
		float2 base = floor(coord);
		float2 f = coord - base;
		float2 f2 = f * f;
		float2 f3 = f2 * f;
		float2 w0 = (1.0 - 3.0 * f + 3.0 * f2 - f3) / 6.0;
		float2 w1 = (4.0 - 6.0 * f2 + 3.0 * f3) / 6.0;
		float2 w3 = f3 / 6.0;
		float2 g0 = w0 + w1;
		float2 g1 = 1.0 - g0;
		float2 t0 = (base - 0.5 + w1 / g0) * texel;
		float2 t1 = (base + 1.5 + w3 / g1) * texel;

		return (image.sample(image_sampler, t0).rgb * g0.x + image.sample(image_sampler, float2(t1.x, t0.y)).rgb * g1.x) * g0.y +
		       (image.sample(image_sampler, float2(t0.x, t1.y)).rgb * g0.x + image.sample(image_sampler, t1).rgb * g1.x) * g1.y;
	}

	fragment float4 ps_backdrop_blur(PixelInput input [[stage_in]], constant BlurConstants& blur [[buffer(1)]], texture2d<float> image [[texture(0)]],
									 sampler image_sampler [[sampler(0)]])
	{
		float3 rgb = sample_blurred(image, image_sampler, input.position.xy / blur.target_size, blur.texel_step);
		float luma = dot(rgb, float3(0.299, 0.587, 0.114));
		rgb = mix(float3(luma), rgb, 1.15) + (backdrop_noise(input.position.xy) - 0.5) * (4.0 / 255.0);
		return float4(saturate(rgb), input.color.a);
	}

	struct FullscreenOutput {
		float4 position [[position]];
		float2 uv;
	};

	vertex FullscreenOutput vs_fullscreen(uint vertex_id [[vertex_id]])
	{
		FullscreenOutput output;
		float2 uv = float2(float((vertex_id << 1) & 2), float(vertex_id & 2));
		output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
		output.uv = uv;
		return output;
	}

	// Halves its source through five taps, a small tent rather than a box, so fine detail does not alias into the level below.
	fragment float4 ps_downsample(FullscreenOutput input [[stage_in]], constant BlurConstants& blur [[buffer(1)]], texture2d<float> image [[texture(0)]],
								  sampler image_sampler [[sampler(0)]])
	{
		float2 diagonal = float2(blur.texel_step.x, -blur.texel_step.y);
		float4 sum = image.sample(image_sampler, input.uv) * 4.0;
		sum += image.sample(image_sampler, input.uv - blur.texel_step) + image.sample(image_sampler, input.uv + blur.texel_step);
		sum += image.sample(image_sampler, input.uv - diagonal) + image.sample(image_sampler, input.uv + diagonal);
		return sum * 0.125;
	}

	fragment float4 ps_blur(FullscreenOutput input [[stage_in]], constant BlurConstants& blur [[buffer(1)]], texture2d<float> image [[texture(0)]],
							sampler image_sampler [[sampler(0)]])
	{
		const float offsets[4] = {0.0, 1.411764706, 3.294117647, 5.176470588};
		const float weights[4] = {0.196482550, 0.296906965, 0.094470398, 0.010381362};

		float4 sum = image.sample(image_sampler, input.uv) * weights[0];
		for (int i = 1; i < 4; i++) {
			sum += image.sample(image_sampler, input.uv + blur.texel_step * offsets[i]) * weights[i];
			sum += image.sample(image_sampler, input.uv - blur.texel_step * offsets[i]) * weights[i];
		}

		return sum;
	}
)";
