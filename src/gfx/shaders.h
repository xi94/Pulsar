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
)";
