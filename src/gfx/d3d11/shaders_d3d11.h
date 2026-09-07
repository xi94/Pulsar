#pragma once

// The HLSL every draw path in renderer_d3d11.cpp compiles from: one shared vertex shader plus
// five pixel shaders selected per draw call. tex0 and sampler0 are declared unconditionally so
// every entry point compiles from the same source, even the ones that never sample.
//
// Kept in its own header so the backend's C++ reads as C++ rather than as a scroll through a
// few hundred lines of embedded shader text.

namespace rift::gfx::d3d11 {
constexpr const char *kShaderSource = R"(
		cbuffer Constants : register(b0) {
			float2 viewport_size;
			float2 padding;
		};

		Texture2D tex0 : register(t0);
		SamplerState sampler0 : register(s0);

		struct VS_Input {
			float2 position : POSITION;
			float2 uv : TEXCOORD0;
			float4 color : COLOR0;
		};

		struct PS_Input {
			float4 position : SV_POSITION;
			float2 uv : TEXCOORD0;
			float4 color : COLOR0;
		};

		PS_Input vs_main(VS_Input input)
		{
			PS_Input output;
			float2 ndc = float2(
				(input.position.x / viewport_size.x) * 2.0 - 1.0,
				1.0 - (input.position.y / viewport_size.y) * 2.0);
			output.position = float4(ndc, 0.0, 1.0);
			output.uv = input.uv;
			output.color = input.color;
			return output;
		}

		float4 ps_main(PS_Input input) : SV_TARGET
		{
			return input.color;
		}

		float4 ps_main_textured(PS_Input input) : SV_TARGET
		{
			return tex0.Sample(sampler0, input.uv) * input.color;
		}

		/// The carousel/grid hover-and-selected glow: a rounded-box signed distance field
		/// locates each pixel relative to the actual card's outline (not the larger quad
		/// this glow is drawn on), so the visible glow band hugs the card's real straight
		/// edges and rounded corners exactly. A thin, dim always-on ring stays visible the
		/// whole time; two brighter "comets" 180 degrees apart continuously orbit the
		/// perimeter together on top of it. Soft smoothstep-shaped lobes (not a sharp pow
		/// spike) keep the motion reading as a smooth, polished wave rather than a hot dot
		/// snapping around. Both are tinted by the game's accent (input.color, set by
		/// the caller, not a global UI color); the card itself gets a plain white border
		/// drawn separately on top, which is what keeps this glow contained to a ring
		/// instead of washing over the card's face.
		cbuffer BannerGlowConstants : register(b1) {
			float bg_time_seconds;
			float bg_quad_width;
			float bg_quad_height;
			float bg_corner_radius;
			float bg_ring_width;
			float3 bg_padding;
		};

		/// Signed distance from point p (card-local, origin at center) to a rounded-corner
		/// box of the given half-size and corner radius - negative inside, positive
		/// outside.
		float rounded_box_sdf(float2 p, float2 half_size, float radius)
		{
			float2 q = abs(p) - half_size + radius;
			return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
		}

		/// Shortest signed angular distance from `a` to `b`, wrapped to -pi..pi - lets a
		/// beam's brightness falloff treat the angle space as a loop instead of snapping
		/// at the +-pi seam.
		float angle_delta(float a, float b)
		{
			float d = a - b;
			return atan2(sin(d), cos(d));
		}

		float4 ps_banner_glow(PS_Input input) : SV_TARGET
		{
			float2 local = (input.uv - 0.5) * float2(bg_quad_width, bg_quad_height);
			float2 card_half_size = float2(bg_quad_width, bg_quad_height) * 0.5 - bg_ring_width;
			float ring = max(bg_ring_width, 0.001);

			/// 0 right at the card's edge, 1 at the glow band's outer edge - the sign
			/// of rounded_box_sdf is negative inside the card (occluded by the opaque card
			/// drawn on top of this, so it doesn't matter what happens there) and positive
			/// outside it, which is exactly the band this glow is meant to fill. A steeper
			/// curve than a plain sqrt keeps the visible band slim, hugging the card's
			/// edge, instead of spreading evenly across the whole margin.
			float d = rounded_box_sdf(local, card_half_size, bg_corner_radius);
			float t = saturate(d / ring);
			float band = pow(saturate(1.0 - t), 1.8);

			float ambient = band * 0.32;

			/// Two comets, 180 degrees apart, orbiting together. Normalizing local by
			/// card_half_size before the atan2 keeps their travel speed visually even
			/// around a non-square card instead of rushing past the short sides.
			float2 norm = local / max(card_half_size, 1.0);
			float angle = atan2(norm.y, norm.x);
			const float pi = 3.14159265;
			float rotation = bg_time_seconds * 1.1;
			float lobe_width = 0.6; // radians - wider reads as a soft wave, not a hard dot
			float lobe_a = 1.0 - smoothstep(0.0, lobe_width, abs(angle_delta(angle, rotation)));
			float lobe_b = 1.0 - smoothstep(0.0, lobe_width, abs(angle_delta(angle, rotation + pi)));
			float comets = max(lobe_a, lobe_b);

			float glow = saturate(ambient + band * comets * 0.85);
			glow *= 1.0 - smoothstep(0.85, 1.0, t); // clean falloff to nothing right at the quad's edge
			return float4(input.color.rgb * glow, input.color.a * glow);
		}

		/// CColorPicker's saturation/value square. color(S,V) = V * lerp(white, hue, S) has
		/// a genuine S*V cross term, so it is *not* reproducible by interpolating 4 corner
		/// colors across a quad's two triangles (that only reconstructs affine functions
		/// exactly, not bilinear ones) - a real per-pixel HSV->RGB conversion is the correct
		/// fix, not another geometry trick. No cbuffer needed: hue travels in via the
		/// vertex color's red channel (0..1 UNORM = 0..360 degrees), set once per quad by
		/// CDrawList::AddRectColorPickerSv rather than varying per pixel.
		float3 hsv_to_rgb(float h, float s, float v)
		{
			float3 rgb = saturate(abs(fmod(h / 60.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
			return v * lerp(float3(1.0, 1.0, 1.0), rgb, s);
		}

		float4 ps_color_picker_sv(PS_Input input) : SV_TARGET
		{
			float hue = input.color.r * 360.0;
			float saturation = input.uv.x;
			float value = 1.0 - input.uv.y;
			return float4(hsv_to_rgb(hue, saturation, value), 1.0);
		}

		/// The account modal's login/progress ring. A single quad (see
		/// CDrawList::AddCircularProgress) carries input.color as the ring/arc's tint
		/// (alpha already folds in the caller's fade) - everything else about the ring's
		/// look is computed per pixel here: a dim always-present track, either a full solid
		/// ring or an animated comet-tail arc with true rounded caps, and a soft outward
		/// glow halo, so the whole thing stays perfectly smooth at any radius instead of
		/// faceting like tessellated geometry would.
		cbuffer CircularProgressConstants : register(b1) {
			float cp_quad_width;
			float cp_quad_height;
			float cp_outer_radius;
			float cp_inner_radius;
			float cp_start_angle;
			float cp_sweep_angle;
			float cp_glow_strength;
			float cp_padding;
		};

		float4 ps_circular_progress(PS_Input input) : SV_TARGET
		{
			float2 local = (input.uv - 0.5) * float2(cp_quad_width, cp_quad_height);
			float r = length(local);
			float angle = atan2(local.y, local.x);
			const float two_pi = 6.28318531;
			const float aa = 1.25; // antialiasing feather, logical pixels

			/// The ring band itself, antialiased on both the inner and outer edge.
			float ring = smoothstep(cp_inner_radius - aa, cp_inner_radius + aa, r) *
						(1.0 - smoothstep(cp_outer_radius - aa, cp_outer_radius + aa, r));

			bool solid = cp_sweep_angle >= two_pi - 0.001;

			float3 track_color = float3(0.16, 0.16, 0.19);
			float track_alpha = ring * 0.9;

			float3 arc_rgb;
			float arc_alpha;
			if (solid) {
				arc_rgb = input.color.rgb;
				arc_alpha = ring;
			} else {
				/// Wrap the pixel's angle into start-relative space, then clamp to the
				/// sweep - this is the arc's parameter, 0 at the tail end, 1 at the
				/// head. Projecting it back onto the centerline and measuring distance from
				/// there (rather than a second angular smoothstep) is what gives the arc's
				/// two ends real rounded caps for free, the same "distance to nearest point
				/// on the shape" idea rounded_box_sdf uses above, just for a curved shape
				/// instead of a box.
				float da = angle - cp_start_angle;
				da = da - two_pi * floor(da / two_pi + 0.5);
				float sweep = max(cp_sweep_angle, 0.0001);
				float s = clamp(da, 0.0, sweep);
				float t = s / sweep;

				float centerline_r = (cp_outer_radius + cp_inner_radius) * 0.5;
				float half_thickness = (cp_outer_radius - cp_inner_radius) * 0.5;
				float2 closest = centerline_r * float2(cos(cp_start_angle + s), sin(cp_start_angle + s));
				float dist = length(local - closest);

				float coverage = 1.0 - smoothstep(half_thickness - aa, half_thickness + aa, dist);
				arc_rgb = input.color.rgb * (0.6 + 0.8 * t); // brightens toward the head
				arc_alpha = coverage * t;					  // fades toward the tail
			}

			/// The arc composited over the track - both still straight, un-premultiplied
			/// alpha.
			float ring_alpha = saturate(arc_alpha + track_alpha * (1.0 - arc_alpha));
			float3 ring_rgb = ring_alpha > 0.0001
				? (arc_rgb * arc_alpha + track_color * track_alpha * (1.0 - arc_alpha)) / ring_alpha
				: track_color;

			/// A soft halo bleeding outward past the ring's edge, plus (while an arc is
			/// active) a brighter bloom concentrated at the comet's leading tip so the head
			/// reads as an actual light source, not just a brighter pixel.
			float outer_margin = max(max(cp_quad_width, cp_quad_height) * 0.5 - cp_outer_radius, 0.001);
			float glow_t = saturate((r - cp_outer_radius) / outer_margin);
			float glow = cp_glow_strength * pow(saturate(1.0 - glow_t), 2.2);
			if (!solid) {
				float head_angle = cp_start_angle + cp_sweep_angle;
				float2 head_pos =
					((cp_outer_radius + cp_inner_radius) * 0.5) * float2(cos(head_angle), sin(head_angle));
				float head_dist = length(local - head_pos);
				/// This Gaussian falloff never truly reaches zero (exp() only decays toward
				/// it), unlike the ambient band's pow(saturate(...)) term above, which is
				/// forced to exactly 0 right at the quad's edge. Left unmasked, that
				/// residual glow gets hard-clipped by the quad boundary instead of fading out
				/// first, which reads as a faint rectangular outline around the ring - so it's
				/// tapered by the same saturate(1.0 - glow_t) envelope the ambient term already
				/// obeys, forcing it to vanish by the same edge instead of getting cut off.
				float head_glow = cp_glow_strength * 0.9 * exp(-(head_dist * head_dist) / (2.0 * 16.0 * 16.0));
				head_glow *= saturate(1.0 - glow_t);
				glow = saturate(glow + head_glow);
			}
			glow *= 1.0 - ring_alpha; // the halo only shows past the ring's opaque edge

			float combined_alpha = saturate(ring_alpha + glow);
			float3 final_rgb = combined_alpha > 0.0001 ? (ring_rgb * ring_alpha + input.color.rgb * glow) / combined_alpha
														: track_color;

			return float4(saturate(final_rgb), combined_alpha * input.color.a);
		}
	)";
} // namespace rift::gfx::d3d11
