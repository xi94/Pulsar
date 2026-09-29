#pragma once

constexpr const char *shader_source = R"(
	cbuffer ViewportConstants : register(b0) {
		float2 viewport_size;
		float2 viewport_padding;
	};

	Texture2D image : register(t0);
	SamplerState image_sampler : register(s0);

	struct VertexInput {
		float2 position : POSITION;
		float2 uv : TEXCOORD0;
		float4 color : COLOR0;
	};

	struct PixelInput {
		float4 position : SV_POSITION;
		float2 uv : TEXCOORD0;
		float4 color : COLOR0;
	};

	PixelInput vs_main(VertexInput input)
	{
		PixelInput output;
		float2 ndc = float2(input.position.x / viewport_size.x * 2.0 - 1.0,
							1.0 - input.position.y / viewport_size.y * 2.0);
		output.position = float4(ndc, 0.0, 1.0);
		output.uv = input.uv;
		output.color = input.color;
		return output;
	}

	float4 ps_solid(PixelInput input) : SV_TARGET
	{
		return input.color;
	}

	float4 ps_textured(PixelInput input) : SV_TARGET
	{
		return image.Sample(image_sampler, input.uv) * input.color;
	}

	cbuffer BannerGlowConstants : register(b1) {
		float glow_time_seconds;
		float glow_quad_width;
		float glow_quad_height;
		float glow_corner_radius;
		float glow_ring_width;
		float3 glow_padding;
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

	float4 ps_banner_glow(PixelInput input) : SV_TARGET
	{
		const float pi = 3.14159265;
		const float comet_width = 0.6;

		float2 local = (input.uv - 0.5) * float2(glow_quad_width, glow_quad_height);
		float2 card_half_size = float2(glow_quad_width, glow_quad_height) * 0.5 - glow_ring_width;
		float ring = max(glow_ring_width, 0.001);

		float distance_out = saturate(rounded_box_distance(local, card_half_size, glow_corner_radius) / ring);
		float band = pow(saturate(1.0 - distance_out), 1.8);
		float ambient = band * 0.32;

		float2 normalized = local / max(card_half_size, 1.0);
		float angle = atan2(normalized.y, normalized.x);
		float rotation = glow_time_seconds * 1.1;
		float comet_a = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation)));
		float comet_b = 1.0 - smoothstep(0.0, comet_width, abs(wrapped_angle_between(angle, rotation + pi)));

		float glow = saturate(ambient + band * max(comet_a, comet_b) * 0.85);
		glow *= 1.0 - smoothstep(0.85, 1.0, distance_out);
		return float4(input.color.rgb * glow, input.color.a * glow);
	}

	cbuffer ShadowConstants : register(b1) {
		float shadow_quad_width;
		float shadow_quad_height;
		float shadow_corner_radius;
		float shadow_blur;
	};

	float approximate_erf(float x)
	{
		float magnitude = abs(x);
		float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * magnitude * magnitude) * magnitude) * magnitude;
		t *= t;
		return sign(x) * (1.0 - 1.0 / (t * t));
	}

	float4 ps_shadow(PixelInput input) : SV_TARGET
	{
		float2 quad_size = float2(shadow_quad_width, shadow_quad_height);
		float2 local = (input.uv - 0.5) * quad_size;
		float edge_distance = rounded_box_distance(local, quad_size * 0.5 - shadow_blur, shadow_corner_radius);
		float sigma = max(shadow_blur * 0.5, 0.001);
		float coverage = 0.5 - 0.5 * approximate_erf(edge_distance / (sigma * 1.41421356));
		return float4(input.color.rgb, input.color.a * coverage);
	}

	float3 hsv_to_rgb(float h, float s, float v)
	{
		float3 rgb = saturate(abs(fmod(h / 60.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
		return v * lerp(float3(1.0, 1.0, 1.0), rgb, s);
	}

	float4 ps_color_picker(PixelInput input) : SV_TARGET
	{
		float hue = input.color.r * 360.0;
		return float4(hsv_to_rgb(hue, input.uv.x, 1.0 - input.uv.y), 1.0);
	}

	cbuffer CircularProgressConstants : register(b1) {
		float ring_quad_width;
		float ring_quad_height;
		float ring_outer_radius;
		float ring_inner_radius;
		float ring_start_angle;
		float ring_sweep_angle;
		float ring_glow_strength;
		uint ring_track_rgba;
	};

	float4 ps_circular_progress(PixelInput input) : SV_TARGET
	{
		const float two_pi = 6.28318531;
		const float feather = 1.25;
		const float3 track_color =
			float3(ring_track_rgba & 0xFF, (ring_track_rgba >> 8) & 0xFF, (ring_track_rgba >> 16) & 0xFF) / 255.0;

		float2 local = (input.uv - 0.5) * float2(ring_quad_width, ring_quad_height);
		float r = length(local);
		float angle = atan2(local.y, local.x);

		float ring = smoothstep(ring_inner_radius - feather, ring_inner_radius + feather, r) *
					 (1.0 - smoothstep(ring_outer_radius - feather, ring_outer_radius + feather, r));
		float track_alpha = ring * 0.9;
		bool full_ring = ring_sweep_angle >= two_pi - 0.001;

		float3 arc_rgb = input.color.rgb;
		float arc_alpha = ring;

		if (!full_ring) {
			float from_start = angle - ring_start_angle;
			from_start -= two_pi * floor(from_start / two_pi + 0.5);

			float sweep = max(ring_sweep_angle, 0.0001);
			float along = clamp(from_start, 0.0, sweep);
			float progress = along / sweep;

			float centerline_radius = (ring_outer_radius + ring_inner_radius) * 0.5;
			float half_thickness = (ring_outer_radius - ring_inner_radius) * 0.5;
			float2 closest = centerline_radius * float2(cos(ring_start_angle + along), sin(ring_start_angle + along));
			float coverage = 1.0 - smoothstep(half_thickness - feather, half_thickness + feather, length(local - closest));

			arc_rgb = input.color.rgb * (0.6 + 0.8 * progress);
			arc_alpha = coverage * progress;
		}

		float ring_alpha = saturate(arc_alpha + track_alpha * (1.0 - arc_alpha));
		float3 ring_rgb = ring_alpha > 0.0001
			? (arc_rgb * arc_alpha + track_color * track_alpha * (1.0 - arc_alpha)) / ring_alpha
			: track_color;

		float glow_margin = max(max(ring_quad_width, ring_quad_height) * 0.5 - ring_outer_radius, 0.001);
		float glow_falloff = saturate(1.0 - saturate((r - ring_outer_radius) / glow_margin));
		float glow = ring_glow_strength * pow(glow_falloff, 2.2);

		if (!full_ring) {
			float head_angle = ring_start_angle + ring_sweep_angle;
			float2 head = ((ring_outer_radius + ring_inner_radius) * 0.5) * float2(cos(head_angle), sin(head_angle));
			float head_distance = length(local - head);
			float head_glow = ring_glow_strength * 0.9 * exp(-(head_distance * head_distance) / (2.0 * 16.0 * 16.0));
			glow = saturate(glow + head_glow * glow_falloff);
		}

		glow *= 1.0 - ring_alpha;

		float combined_alpha = saturate(ring_alpha + glow);
		float3 combined_rgb = combined_alpha > 0.0001
			? (ring_rgb * ring_alpha + input.color.rgb * glow) / combined_alpha
			: track_color;

		return float4(saturate(combined_rgb), combined_alpha * input.color.a);
	}

	cbuffer OutlineCountdownConstants : register(b1) {
		float outline_quad_width;
		float outline_quad_height;
		float outline_half_width;
		float outline_half_height;
		float outline_radius;
		float outline_thickness;
		float outline_glow_radius;
		float outline_lit_length;
		float2 outline_end;
		float2 outline_padding;
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

	float4 ps_outline_countdown(PixelInput input) : SV_TARGET
	{
		const float pi = 3.14159265;
		float2 local = (input.uv - 0.5) * float2(outline_quad_width, outline_quad_height);
		float radius = outline_radius;
		float2 core = max(float2(outline_half_width, outline_half_height) - radius, 0.0);

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
		float end_distance = length(local - outline_end);

		float line_distance = path_distance;
		float progress = saturate(position / max(outline_lit_length, 0.0001));

		if (position > outline_lit_length) {
			line_distance = min(start_distance, end_distance);
			progress = smoothstep(-1.0, 1.0, start_distance - end_distance);
		}

		float half_thickness = outline_thickness * 0.5;
		float footprint = max(fwidth(line_distance), 0.0001) * 1.25;
		float coverage = saturate((half_thickness - line_distance) / footprint + 0.5);
		float line_alpha = lerp(coverage, sqrt(coverage), 0.35);

		float emphasis = progress * progress;
		float3 base = input.color.rgb;
		float3 line_rgb = lerp(base * 0.85, lerp(base, float3(1.0, 0.62, 0.7), 0.3), emphasis);

		float glow_distance = max(line_distance - half_thickness, 0.0);
		float glow_falloff = exp(-(glow_distance * glow_distance) / (2.0 * outline_glow_radius * outline_glow_radius));
		float glow = (0.12 + 0.12 * emphasis) * glow_falloff * (1.0 - line_alpha);

		float alpha = saturate(line_alpha + glow);
		float3 rgb = alpha > 0.0001 ? (line_rgb * line_alpha + base * glow) / alpha : base;

		return float4(saturate(rgb), alpha * input.color.a);
	}

	cbuffer BackdropConstants : register(b1) {
		float2 backdrop_target_size;
		float backdrop_intensity;
		float backdrop_style;
		float backdrop_pixel_scale;
		float3 backdrop_padding;
	};

	static const float backdrop_pattern_strength[6] = {0.0, 0.0, 0.2, 0.06, 0.24, 0.05};
	static const float backdrop_pattern_spacing[6] = {1.0, 1.0, 24.0, 40.0, 48.0, 10.0};

	float backdrop_noise(float2 pixel)
	{
		float3 p = frac(pixel.xyx * 0.1031);
		p += dot(p, p.yzx + 33.33);
		return frac((p.x + p.y) * p.z);
	}

	float backdrop_pattern(float2 pixel, float2 size, float scale, int style)
	{
		float spacing = max(round(backdrop_pattern_spacing[style] * scale), 2.0);
		float stroke = max(round(scale), 1.0);
		float arm = round(5.0 * scale);

		float2 cell = floor(pixel) - floor(size * 0.5);
		float2 m = cell - spacing * floor(cell / spacing);
		float diagonal = cell.x + cell.y;
		float slant = diagonal - spacing * floor(diagonal / spacing);

		bool on_dot = m.x < stroke && m.y < stroke;
		bool on_grid = m.x < stroke || m.y < stroke;
		bool across = m.y < stroke && (m.x < arm + stroke || m.x >= spacing - arm);
		bool upright = m.x < stroke && (m.y < arm + stroke || m.y >= spacing - arm);
		bool on_line = slant < stroke;

		bool hit = false;
		hit = style == 2 ? on_dot : hit;
		hit = style == 3 ? on_grid : hit;
		hit = style == 4 ? (across || upright) : hit;
		hit = style == 5 ? on_line : hit;

		return hit ? 1.0 : 0.0;
	}

	float4 ps_backdrop(PixelInput input) : SV_TARGET
	{
		int style = clamp((int)(backdrop_style + 0.5), 0, 5);
		float2 pixel = input.position.xy;
		float2 size = backdrop_target_size;
		float k = style == 0 ? 0.0 : backdrop_intensity;

		float light_distance = length(pixel - float2(size.x * 0.5, -size.y * 0.2));
		float light = saturate(1.0 - light_distance / (size.y * 1.3));

		float vignette_start = size.y * 0.3;
		float vignette_span = max(size.x * 0.75 - vignette_start, 1.0);
		float vignette = saturate((length(pixel - size * 0.5) - vignette_start) / vignette_span);

		float pattern = backdrop_pattern(pixel, size, max(backdrop_pixel_scale, 0.5), style);
		float reveal = 0.4 + 0.6 * smoothstep(0.0, 0.8, light);
		float3 ink = dot(input.color.rgb, float3(0.299, 0.587, 0.114)) > 0.5 ? 0.0 : 1.0;

		float3 rgb = input.color.rgb;
		rgb = lerp(rgb, 1.0, light * 0.07 * k);
		rgb *= 1.0 - vignette * 0.25 * k;
		rgb = lerp(rgb, ink, pattern * backdrop_pattern_strength[style] * k * reveal);
		rgb += (backdrop_noise(pixel) - 0.5) * (8.0 / 255.0) * k;

		return float4(saturate(rgb), input.color.a);
	}
)";
