#include "render/render_backend.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "core/debug_log.h"
#include "core/profiler.h"
#include "gfx/draw_list.h"
#include "os/window.h"
#include "render/opengl/context.h"
#include "render/opengl/gl.h"
#include "render/opengl/shaders.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "gfx";

constexpr GLsizei K_MSAA_SAMPLE_COUNT  = 4;
constexpr GLuint  K_VIEWPORT_BINDING   = 0;
constexpr GLuint  K_EFFECT_BINDING     = 1;
constexpr GLuint  K_POSITION_ATTRIBUTE = 0;
constexpr GLuint  K_UV_ATTRIBUTE       = 1;
constexpr GLuint  K_COLOR_ATTRIBUTE    = 2;

constexpr const char* K_FRAGMENT_ENTRY_POINTS[]{
	"ps_solid", "ps_textured", "ps_banner_glow", "ps_color_picker", "ps_shadow", "ps_outline_countdown", "ps_backdrop", "ps_backdrop_plain",
};

static_assert(std::size(K_FRAGMENT_ENTRY_POINTS) == K_SHADER_KIND_COUNT);

constexpr const char* K_EFFECT_BLOCK_NAMES[]{"BannerGlowConstants", "ShadowConstants", "OutlineCountdownConstants", "BackdropConstants"};

[[nodiscard]] auto load_functions(Gl* t_gl, const GlContext& t_context) -> bool
{
	bool complete = true;

#define PULSAR_GL_LOAD(return_type, name, symbol, ...)                                                                                                         \
	t_gl->name = reinterpret_cast<return_type (*)(__VA_ARGS__)>(t_context.proc_address(#symbol));                                                              \
	complete   = complete && t_gl->name != nullptr;
	PULSAR_GL_FUNCTIONS(PULSAR_GL_LOAD)
#undef PULSAR_GL_LOAD

	return complete;
}

[[nodiscard]] auto buffer_offset(usize t_offset) -> const void*
{
	return reinterpret_cast<const void*>(t_offset);
}

struct OpenGlBackend final : RenderBackend {
	GlContext context;
	Gl        gl;
	bool      functions_loaded = false;

	GLuint  programs[K_SHADER_KIND_COUNT]{};
	GLuint  vertex_array     = 0;
	GLuint  vertex_buffer    = 0;
	GLuint  index_buffer     = 0;
	GLuint  viewport_buffer  = 0;
	GLuint  effect_buffer    = 0;
	GLuint  sampler          = 0;
	GLuint  msaa_framebuffer = 0;
	GLuint  msaa_color       = 0;
	GLsizei msaa_samples     = 0;
	GLsizei width            = 0;
	GLsizei height           = 0;

	GLuint textures[Renderer::K_MAX_TEXTURES]{};

	~OpenGlBackend() override;

	[[nodiscard]] auto init(const os::Window* t_window) -> bool override;
	auto resize(u32 t_physical_width, u32 t_physical_height) -> void override;
	auto render(const RenderFrame& t_frame) -> void override;

	[[nodiscard]] auto create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool override;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void override;
	auto destroy_texture(u32 t_slot) -> void override;

	[[nodiscard]] auto compile_shader(GLenum t_type, std::span<const char* const> t_sources) const -> GLuint;
	[[nodiscard]] auto link_program(GLuint t_vertex_shader, GLuint t_fragment_shader) const -> GLuint;
	[[nodiscard]] auto create_programs() -> bool;
	[[nodiscard]] auto create_buffers() -> bool;
	auto bind_block(GLuint t_program, const char* t_name, GLuint t_binding) const -> void;
	auto create_msaa_target() -> void;
	auto size_msaa_target() -> void;
	auto draw_command(const RenderFrame& t_frame, const DrawCommand& t_command) -> void;
};

OpenGlBackend::~OpenGlBackend()
{
	if (!functions_loaded) return;

	for (const GLuint program : programs) {
		gl.delete_program(program);
	}

	const GLuint buffers[]{vertex_buffer, index_buffer, viewport_buffer, effect_buffer};
	gl.delete_buffers(static_cast<GLsizei>(std::size(buffers)), buffers);
	gl.delete_textures(static_cast<GLsizei>(std::size(textures)), textures);
	gl.delete_vertex_arrays(1, &vertex_array);
	gl.delete_samplers(1, &sampler);
	gl.delete_framebuffers(1, &msaa_framebuffer);
	gl.delete_renderbuffers(1, &msaa_color);
}

auto OpenGlBackend::init(const os::Window* t_window) -> bool
{
	const auto started = std::chrono::steady_clock::now();
	if (!context.create(t_window)) return false;

	functions_loaded = load_functions(&gl, context);
	if (!functions_loaded) {
		debug_log::write(K_LOG_CATEGORY, "the OpenGL driver lacks functions Pulsar needs");
		return false;
	}

	debug_log::write(K_LOG_CATEGORY, "OpenGL %s on %s", reinterpret_cast<const char*>(gl.get_string(GL_VERSION)),
	                 reinterpret_cast<const char*>(gl.get_string(GL_RENDERER)));

	width  = static_cast<GLsizei>(t_window->physical_width());
	height = static_cast<GLsizei>(t_window->physical_height());

	if (!create_programs() || !create_buffers()) return false;

	create_msaa_target();
	gl.enable(GL_BLEND);
	gl.blend_func_separate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	debug_log::write(K_LOG_CATEGORY, "OpenGL ready in %.1f ms (%d samples)",
	                 std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count(), msaa_framebuffer != 0 ? msaa_samples : 1);

	return true;
}

auto OpenGlBackend::resize(u32 t_physical_width, u32 t_physical_height) -> void
{
	width  = static_cast<GLsizei>(t_physical_width);
	height = static_cast<GLsizei>(t_physical_height);

	context.resize();
	size_msaa_target();
}

auto OpenGlBackend::render(const RenderFrame& t_frame) -> void
{
	{
		PULSAR_PROFILE_SCOPE("Render.Submit");

		const Color clear = t_frame.clear_color;
		gl.bind_framebuffer(GL_FRAMEBUFFER, msaa_framebuffer);
		gl.viewport(0, 0, width, height);
		gl.disable(GL_SCISSOR_TEST);
		gl.clear_color(clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f);
		gl.clear(GL_COLOR_BUFFER_BIT);

		const DrawList* draw_list = t_frame.draw_list;

		if (!draw_list->commands().empty()) {
			const auto              vertices = draw_list->vertices();
			const auto              indices  = draw_list->indices();
			const ViewportConstants viewport = viewport_constants(t_frame);

			gl.bind_buffer(GL_ARRAY_BUFFER, vertex_buffer);
			gl.buffer_data(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STREAM_DRAW);
			gl.buffer_data(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(), GL_STREAM_DRAW);
			gl.bind_buffer(GL_UNIFORM_BUFFER, viewport_buffer);
			gl.buffer_sub_data(GL_UNIFORM_BUFFER, 0, sizeof(viewport), &viewport);

			gl.enable(GL_SCISSOR_TEST);
			for (const DrawCommand& command : draw_list->commands()) {
				draw_command(t_frame, command);
			}
		}

		if (msaa_framebuffer != 0) {
			gl.disable(GL_SCISSOR_TEST);
			gl.bind_framebuffer(GL_READ_FRAMEBUFFER, msaa_framebuffer);
			gl.bind_framebuffer(GL_DRAW_FRAMEBUFFER, 0);
			gl.blit_framebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}

	{
		PULSAR_PROFILE_SCOPE("Render.Present");
		context.present();
	}
}

auto OpenGlBackend::create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool) -> bool
{
	GLuint texture = 0;
	gl.gen_textures(1, &texture);
	if (texture == 0) return false;

	gl.bind_texture(GL_TEXTURE_2D, texture);
	gl.tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(t_levels.size()) - 1);

	for (usize level = 0; level < t_levels.size(); level += 1) {
		const TextureLevel& pixels = t_levels[level];
		gl.tex_image_2d(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA8, static_cast<GLsizei>(pixels.width), static_cast<GLsizei>(pixels.height), 0, GL_RGBA,
		                GL_UNSIGNED_BYTE, pixels.rgba_pixels);
	}

	textures[t_slot] = texture;

	return true;
}

auto OpenGlBackend::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	gl.bind_texture(GL_TEXTURE_2D, textures[t_slot]);
	gl.tex_sub_image_2d(GL_TEXTURE_2D, 0, static_cast<GLint>(t_x), static_cast<GLint>(t_y), static_cast<GLsizei>(t_width), static_cast<GLsizei>(t_height),
	                    GL_RGBA, GL_UNSIGNED_BYTE, t_rgba_pixels);
}

auto OpenGlBackend::destroy_texture(u32 t_slot) -> void
{
	gl.delete_textures(1, &textures[t_slot]);
	textures[t_slot] = 0;
}

auto OpenGlBackend::compile_shader(GLenum t_type, std::span<const char* const> t_sources) const -> GLuint
{
	const GLuint shader = gl.create_shader(t_type);
	gl.shader_source(shader, static_cast<GLsizei>(t_sources.size()), t_sources.data(), nullptr);
	gl.compile_shader(shader);

	GLint compiled = GL_FALSE;
	gl.get_shaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_FALSE) return shader;

	GLint length = 0;
	gl.get_shaderiv(shader, GL_INFO_LOG_LENGTH, &length);

	std::string log(static_cast<usize>(std::max(length, 1)), '\0');
	gl.get_shader_info_log(shader, length, nullptr, log.data());
	debug_log::write(K_LOG_CATEGORY, "GLSL compile failed: %s", log.c_str());

	gl.delete_shader(shader);

	return 0;
}

auto OpenGlBackend::link_program(GLuint t_vertex_shader, GLuint t_fragment_shader) const -> GLuint
{
	const GLuint program = gl.create_program();
	gl.attach_shader(program, t_vertex_shader);
	gl.attach_shader(program, t_fragment_shader);
	gl.link_program(program);

	GLint linked = GL_FALSE;
	gl.get_programiv(program, GL_LINK_STATUS, &linked);

	if (linked == GL_FALSE) {
		GLint length = 0;
		gl.get_programiv(program, GL_INFO_LOG_LENGTH, &length);

		std::string log(static_cast<usize>(std::max(length, 1)), '\0');
		gl.get_program_info_log(program, length, nullptr, log.data());
		debug_log::write(K_LOG_CATEGORY, "GLSL link failed: %s", log.c_str());

		gl.delete_program(program);
		return 0;
	}

	bind_block(program, "ViewportConstants", K_VIEWPORT_BINDING);
	for (const char* name : K_EFFECT_BLOCK_NAMES) {
		bind_block(program, name, K_EFFECT_BINDING);
	}

	return program;
}

auto OpenGlBackend::bind_block(GLuint t_program, const char* t_name, GLuint t_binding) const -> void
{
	const GLuint block = gl.get_uniform_block_index(t_program, t_name);

	if (block != GL_INVALID_INDEX) {
		gl.uniform_block_binding(t_program, block, t_binding);
	}
}

auto OpenGlBackend::create_programs() -> bool
{
	const char* const vertex_sources[]{K_GLSL_VERSION, K_VERTEX_SHADER_SOURCE};
	const GLuint      vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_sources);
	if (vertex_shader == 0) return false;

	bool linked_all = true;

	for (u32 i = 0; i < K_SHADER_KIND_COUNT && linked_all; i += 1) {
		const std::string entry_point = std::string{"#define PS_MAIN "} + K_FRAGMENT_ENTRY_POINTS[i] + "\n";
		const char* const fragment_sources[]{K_GLSL_VERSION, entry_point.c_str(), K_FRAGMENT_SHADER_SOURCE};
		const GLuint      fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_sources);

		programs[i] = fragment_shader != 0 ? link_program(vertex_shader, fragment_shader) : 0;
		linked_all  = programs[i] != 0;

		gl.delete_shader(fragment_shader);
	}

	gl.delete_shader(vertex_shader);

	return linked_all;
}

auto OpenGlBackend::create_buffers() -> bool
{
	gl.gen_vertex_arrays(1, &vertex_array);
	gl.bind_vertex_array(vertex_array);

	gl.gen_buffers(1, &vertex_buffer);
	gl.gen_buffers(1, &index_buffer);
	gl.bind_buffer(GL_ARRAY_BUFFER, vertex_buffer);
	gl.bind_buffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

	gl.enable_vertex_attrib_array(K_POSITION_ATTRIBUTE);
	gl.vertex_attrib_pointer(K_POSITION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), buffer_offset(offsetof(Vertex2D, x)));
	gl.enable_vertex_attrib_array(K_UV_ATTRIBUTE);
	gl.vertex_attrib_pointer(K_UV_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), buffer_offset(offsetof(Vertex2D, u)));
	gl.enable_vertex_attrib_array(K_COLOR_ATTRIBUTE);
	gl.vertex_attrib_pointer(K_COLOR_ATTRIBUTE, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), buffer_offset(offsetof(Vertex2D, color)));

	gl.gen_buffers(1, &viewport_buffer);
	gl.bind_buffer(GL_UNIFORM_BUFFER, viewport_buffer);
	gl.buffer_data(GL_UNIFORM_BUFFER, sizeof(ViewportConstants), nullptr, GL_STREAM_DRAW);
	gl.bind_buffer_base(GL_UNIFORM_BUFFER, K_VIEWPORT_BINDING, viewport_buffer);

	gl.gen_buffers(1, &effect_buffer);
	gl.bind_buffer(GL_UNIFORM_BUFFER, effect_buffer);
	gl.buffer_data(GL_UNIFORM_BUFFER, sizeof(EffectConstants), nullptr, GL_STREAM_DRAW);
	gl.bind_buffer_base(GL_UNIFORM_BUFFER, K_EFFECT_BINDING, effect_buffer);

	gl.gen_samplers(1, &sampler);
	gl.sampler_parameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gl.sampler_parameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl.sampler_parameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.sampler_parameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl.bind_sampler(0, sampler);

	return vertex_array != 0 && vertex_buffer != 0 && index_buffer != 0 && viewport_buffer != 0 && effect_buffer != 0 && sampler != 0;
}

auto OpenGlBackend::create_msaa_target() -> void
{
	GLint max_samples = 0;
	gl.get_integerv(GL_MAX_SAMPLES, &max_samples);

	msaa_samples = std::min(K_MSAA_SAMPLE_COUNT, static_cast<GLsizei>(max_samples));
	if (msaa_samples < 2) return;

	gl.gen_framebuffers(1, &msaa_framebuffer);
	gl.gen_renderbuffers(1, &msaa_color);
	size_msaa_target();
}

auto OpenGlBackend::size_msaa_target() -> void
{
	if (msaa_framebuffer == 0) return;

	gl.bind_renderbuffer(GL_RENDERBUFFER, msaa_color);
	gl.renderbuffer_storage_multisample(GL_RENDERBUFFER, msaa_samples, GL_RGBA8, width, height);
	gl.bind_framebuffer(GL_FRAMEBUFFER, msaa_framebuffer);
	gl.framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaa_color);

	if (gl.check_framebuffer_status(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) return;

	debug_log::write(K_LOG_CATEGORY, "the multisampled framebuffer is incomplete - drawing without MSAA");

	gl.bind_framebuffer(GL_FRAMEBUFFER, 0);
	gl.delete_framebuffers(1, &msaa_framebuffer);
	gl.delete_renderbuffers(1, &msaa_color);
	msaa_framebuffer = 0;
	msaa_color       = 0;
}

auto OpenGlBackend::draw_command(const RenderFrame& t_frame, const DrawCommand& t_command) -> void
{
	GLuint image = 0;

	if (t_command.shader == ShaderKind::TEXTURED) {
		if (!t_command.texture->is_valid()) return;

		image = textures[t_command.texture->slot()];
	}

	EffectConstants effect{};
	const usize     effect_size = effect_constants(t_frame, t_command, &effect);

	if (effect_size > 0) {
		gl.bind_buffer(GL_UNIFORM_BUFFER, effect_buffer);
		gl.buffer_sub_data(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(effect_size), &effect);
	}

	const ScissorRect clip = scissor_for(t_frame, t_command);
	gl.scissor(clip.left, height - clip.bottom, clip.right - clip.left, clip.bottom - clip.top);

	gl.use_program(programs[static_cast<u32>(t_command.shader)]);
	gl.bind_texture(GL_TEXTURE_2D, image);
	gl.draw_elements(GL_TRIANGLES, static_cast<GLsizei>(t_command.index_count), GL_UNSIGNED_INT, buffer_offset(t_command.index_offset * sizeof(u32)));
}
}

auto create_opengl_render_backend() -> std::unique_ptr<RenderBackend>
{
	return std::make_unique<OpenGlBackend>();
}
