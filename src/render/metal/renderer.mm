#include "render/render_backend.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "core/debug_log.h"
#include "core/profiler.h"
#include "gfx/draw_list.h"
#include "os/macos/macos.h"
#include "os/window.h"
#include "render/metal/shaders.h"

namespace {
constexpr const char*    K_LOG_CATEGORY           = "gfx";
constexpr NSUInteger     K_MSAA_SAMPLE_COUNT      = 4;
constexpr u32            K_FRAMES_IN_FLIGHT       = 3;
constexpr MTLPixelFormat K_PIXEL_FORMAT           = MTLPixelFormatBGRA8Unorm;
constexpr NSUInteger     K_VERTEX_BUFFER_INDEX    = 0;
constexpr NSUInteger     K_CONSTANTS_BUFFER_INDEX = 1;

constexpr const char* K_FRAGMENT_FUNCTIONS[]{
	"ps_solid", "ps_textured", "ps_banner_glow", "ps_color_picker", "ps_shadow", "ps_outline_countdown", "ps_backdrop", "ps_backdrop_plain",
};

static_assert(std::size(K_FRAGMENT_FUNCTIONS) == K_SHADER_KIND_COUNT);

struct FrameGeometry {
	id<MTLBuffer> vertices;
	id<MTLBuffer> indices;
};

struct MetalBackend final : RenderBackend {
	const os::Window*          window           = nullptr;
	NSView*                    view             = nil;
	id<MTLDevice>              device           = nil;
	id<MTLCommandQueue>        queue            = nil;
	CAMetalLayer*              layer            = nil;
	id<MTLSamplerState>        sampler          = nil;
	id<MTLTexture>             msaa_target      = nil;
	dispatch_semaphore_t       frames_available = nil;
	id<MTLRenderPipelineState> pipelines[K_SHADER_KIND_COUNT]{};
	FrameGeometry              frames[K_FRAMES_IN_FLIGHT]{};
	u32                        frame_index = 0;

	id<MTLTexture> textures[Renderer::K_MAX_TEXTURES]{};

	~MetalBackend() override;

	[[nodiscard]] auto init(const os::Window* t_window) -> bool override;
	auto resize(u32 t_physical_width, u32 t_physical_height) -> void override;
	auto render(const RenderFrame& t_frame) -> void override;

	[[nodiscard]] auto create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool override;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void override;
	auto destroy_texture(u32 t_slot) -> void override;

	[[nodiscard]] auto create_pipelines() -> bool;
	auto create_sampler() -> void;
	auto attach_layer(const os::Window* t_window) -> void;
	auto wait_for_gpu() const -> void;
	auto upload_geometry(FrameGeometry* t_geometry, const DrawList* t_draw_list) const -> void;
	auto draw_command(id<MTLRenderCommandEncoder> t_encoder, const RenderFrame& t_frame, const DrawCommand& t_command, id<MTLBuffer> t_indices) const -> void;
};

MetalBackend::~MetalBackend()
{
	if (frames_available != nil) {
		wait_for_gpu();
	}

	if (view != nil && view.layer == layer) {
		view.layer = nil;
	}
}

auto MetalBackend::init(const os::Window* t_window) -> bool
{
	const auto started = std::chrono::steady_clock::now();

	device = MTLCreateSystemDefaultDevice();
	if (device == nil) {
		debug_log::write(K_LOG_CATEGORY, "this Mac reports no Metal device");
		return false;
	}

	window           = t_window;
	queue            = [device newCommandQueue];
	frames_available = dispatch_semaphore_create(K_FRAMES_IN_FLIGHT);
	if (queue == nil || !create_pipelines()) return false;

	create_sampler();
	attach_layer(t_window);
	resize(t_window->physical_width(), t_window->physical_height());

	debug_log::write(K_LOG_CATEGORY, "Metal on %s ready in %.1f ms", device.name.UTF8String,
	                 std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count());

	return true;
}

auto MetalBackend::resize(u32 t_physical_width, u32 t_physical_height) -> void
{
	layer.contentsScale = window->dpi_scale();
	layer.drawableSize  = CGSizeMake(t_physical_width, t_physical_height);

	MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:K_PIXEL_FORMAT
																						  width:t_physical_width
																						 height:t_physical_height
																					  mipmapped:NO];
	descriptor.textureType           = MTLTextureType2DMultisample;
	descriptor.sampleCount           = K_MSAA_SAMPLE_COUNT;
	descriptor.usage                 = MTLTextureUsageRenderTarget;
	descriptor.storageMode           = MTLStorageModePrivate;

	msaa_target = [device newTextureWithDescriptor:descriptor];
}

auto MetalBackend::render(const RenderFrame& t_frame) -> void
{
	@autoreleasepool {
		id<CAMetalDrawable> drawable = nil;

		{
			PULSAR_PROFILE_SCOPE("Render.Present");
			dispatch_semaphore_wait(frames_available, DISPATCH_TIME_FOREVER);
			drawable = [layer nextDrawable];
		}

		if (drawable == nil || msaa_target == nil) {
			dispatch_semaphore_signal(frames_available);
			return;
		}

		PULSAR_PROFILE_SCOPE("Render.Submit");

		const Color              clear          = t_frame.clear_color;
		MTLRenderPassDescriptor* pass           = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture        = msaa_target;
		pass.colorAttachments[0].resolveTexture = drawable.texture;
		pass.colorAttachments[0].loadAction     = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction    = MTLStoreActionMultisampleResolve;
		pass.colorAttachments[0].clearColor     = MTLClearColorMake(clear.r / 255.0, clear.g / 255.0, clear.b / 255.0, clear.a / 255.0);

		id<MTLCommandBuffer>        commands  = [queue commandBuffer];
		id<MTLRenderCommandEncoder> encoder   = [commands renderCommandEncoderWithDescriptor:pass];
		const DrawList*             draw_list = t_frame.draw_list;

		if (!draw_list->commands().empty()) {
			FrameGeometry*          geometry = &frames[frame_index];
			const ViewportConstants viewport = viewport_constants(t_frame);
			upload_geometry(geometry, draw_list);

			[encoder setVertexBuffer:geometry->vertices offset:0 atIndex:K_VERTEX_BUFFER_INDEX];
			[encoder setVertexBytes:&viewport length:sizeof(viewport) atIndex:K_CONSTANTS_BUFFER_INDEX];
			[encoder setFragmentSamplerState:sampler atIndex:0];

			for (const DrawCommand& command : draw_list->commands()) {
				draw_command(encoder, t_frame, command, geometry->indices);
			}
		}

		[encoder endEncoding];
		[commands presentDrawable:drawable];

		dispatch_semaphore_t semaphore = frames_available;
		[commands addCompletedHandler:^(id<MTLCommandBuffer>) {
			dispatch_semaphore_signal(semaphore);
		}];
		[commands commit];

		frame_index = (frame_index + 1) % K_FRAMES_IN_FLIGHT;
	}
}

auto MetalBackend::create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool) -> bool
{
	const TextureLevel&   base       = t_levels.front();
	MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																						  width:base.width
																						 height:base.height
																					  mipmapped:t_levels.size() > 1];
	descriptor.mipmapLevelCount      = t_levels.size();
	descriptor.usage                 = MTLTextureUsageShaderRead;

	id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
	if (texture == nil) return false;

	for (usize level = 0; level < t_levels.size(); level += 1) {
		const TextureLevel& pixels = t_levels[level];
		[texture replaceRegion:MTLRegionMake2D(0, 0, pixels.width, pixels.height) mipmapLevel:level withBytes:pixels.rgba_pixels bytesPerRow:pixels.width * 4];
	}

	textures[t_slot] = texture;

	return true;
}

auto MetalBackend::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	wait_for_gpu();
	[textures[t_slot] replaceRegion:MTLRegionMake2D(t_x, t_y, t_width, t_height) mipmapLevel:0 withBytes:t_rgba_pixels bytesPerRow:t_width * 4];
}

auto MetalBackend::destroy_texture(u32 t_slot) -> void
{
	textures[t_slot] = nil;
}

auto MetalBackend::create_pipelines() -> bool
{
	NSError*       error   = nil;
	id<MTLLibrary> library = [device newLibraryWithSource:@(K_SHADER_SOURCE) options:[[MTLCompileOptions alloc] init] error:&error];

	if (library == nil) {
		debug_log::write(K_LOG_CATEGORY, "Metal shader compile failed: %s", error.localizedDescription.UTF8String);
		return false;
	}

	MTLVertexDescriptor* vertex_layout                  = [MTLVertexDescriptor vertexDescriptor];
	vertex_layout.attributes[0].format                  = MTLVertexFormatFloat2;
	vertex_layout.attributes[0].offset                  = offsetof(Vertex2D, x);
	vertex_layout.attributes[0].bufferIndex             = K_VERTEX_BUFFER_INDEX;
	vertex_layout.attributes[1].format                  = MTLVertexFormatFloat2;
	vertex_layout.attributes[1].offset                  = offsetof(Vertex2D, u);
	vertex_layout.attributes[1].bufferIndex             = K_VERTEX_BUFFER_INDEX;
	vertex_layout.attributes[2].format                  = MTLVertexFormatUChar4Normalized;
	vertex_layout.attributes[2].offset                  = offsetof(Vertex2D, color);
	vertex_layout.attributes[2].bufferIndex             = K_VERTEX_BUFFER_INDEX;
	vertex_layout.layouts[K_VERTEX_BUFFER_INDEX].stride = sizeof(Vertex2D);

	MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	descriptor.vertexFunction               = [library newFunctionWithName:@"vs_main"];
	descriptor.vertexDescriptor             = vertex_layout;
	descriptor.rasterSampleCount            = K_MSAA_SAMPLE_COUNT;

	MTLRenderPipelineColorAttachmentDescriptor* color = descriptor.colorAttachments[0];
	color.pixelFormat                                 = K_PIXEL_FORMAT;
	color.blendingEnabled                             = YES;
	color.rgbBlendOperation                           = MTLBlendOperationAdd;
	color.alphaBlendOperation                         = MTLBlendOperationAdd;
	color.sourceRGBBlendFactor                        = MTLBlendFactorSourceAlpha;
	color.destinationRGBBlendFactor                   = MTLBlendFactorOneMinusSourceAlpha;
	color.sourceAlphaBlendFactor                      = MTLBlendFactorOne;
	color.destinationAlphaBlendFactor                 = MTLBlendFactorOneMinusSourceAlpha;

	for (u32 i = 0; i < K_SHADER_KIND_COUNT; i += 1) {
		descriptor.fragmentFunction = [library newFunctionWithName:@(K_FRAGMENT_FUNCTIONS[i])];
		pipelines[i]                = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];

		if (pipelines[i] == nil) {
			debug_log::write(K_LOG_CATEGORY, "Metal pipeline %s failed: %s", K_FRAGMENT_FUNCTIONS[i], error.localizedDescription.UTF8String);
			return false;
		}
	}

	return true;
}

auto MetalBackend::create_sampler() -> void
{
	MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
	descriptor.minFilter             = MTLSamplerMinMagFilterLinear;
	descriptor.magFilter             = MTLSamplerMinMagFilterLinear;
	descriptor.mipFilter             = MTLSamplerMipFilterLinear;
	descriptor.sAddressMode          = MTLSamplerAddressModeClampToEdge;
	descriptor.tAddressMode          = MTLSamplerAddressModeClampToEdge;

	sampler = [device newSamplerStateWithDescriptor:descriptor];
}

auto MetalBackend::attach_layer(const os::Window* t_window) -> void
{
	layer                 = [CAMetalLayer layer];
	layer.device          = device;
	layer.pixelFormat     = K_PIXEL_FORMAT;
	layer.framebufferOnly = YES;
	layer.opaque          = YES;

	const CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	layer.colorspace           = srgb;
	CGColorSpaceRelease(srgb);

	view            = os::macos::window_handle(t_window).contentView;
	view.layer      = layer;
	view.wantsLayer = YES;
}

auto MetalBackend::wait_for_gpu() const -> void
{
	for (u32 i = 0; i < K_FRAMES_IN_FLIGHT; i += 1) {
		dispatch_semaphore_wait(frames_available, DISPATCH_TIME_FOREVER);
	}

	for (u32 i = 0; i < K_FRAMES_IN_FLIGHT; i += 1) {
		dispatch_semaphore_signal(frames_available);
	}
}

auto MetalBackend::upload_geometry(FrameGeometry* t_geometry, const DrawList* t_draw_list) const -> void
{
	const auto vertices = t_draw_list->vertices();
	const auto indices  = t_draw_list->indices();

	if (t_geometry->vertices.length < vertices.size_bytes()) {
		t_geometry->vertices = [device newBufferWithLength:vertices.size_bytes() * 2 options:MTLResourceStorageModeShared];
	}

	if (t_geometry->indices.length < indices.size_bytes()) {
		t_geometry->indices = [device newBufferWithLength:indices.size_bytes() * 2 options:MTLResourceStorageModeShared];
	}

	std::memcpy(t_geometry->vertices.contents, vertices.data(), vertices.size_bytes());
	std::memcpy(t_geometry->indices.contents, indices.data(), indices.size_bytes());
}

auto MetalBackend::draw_command(id<MTLRenderCommandEncoder> t_encoder, const RenderFrame& t_frame, const DrawCommand& t_command, id<MTLBuffer> t_indices) const
	-> void
{
	id<MTLTexture> image = nil;

	if (t_command.shader == ShaderKind::Textured) {
		if (!t_command.texture->is_valid()) return;

		image = textures[t_command.texture->slot()];
	}

	EffectConstants effect{};
	const usize     effect_size = effect_constants(t_frame, t_command, &effect);

	if (effect_size > 0) {
		[t_encoder setFragmentBytes:&effect length:effect_size atIndex:K_CONSTANTS_BUFFER_INDEX];
	}

	const ScissorRect clip = scissor_for(t_frame, t_command);
	[t_encoder setScissorRect:MTLScissorRect{
								  .x      = static_cast<NSUInteger>(clip.left),
								  .y      = static_cast<NSUInteger>(clip.top),
								  .width  = static_cast<NSUInteger>(clip.right - clip.left),
								  .height = static_cast<NSUInteger>(clip.bottom - clip.top),
							  }];

	[t_encoder setRenderPipelineState:pipelines[static_cast<u32>(t_command.shader)]];
	[t_encoder setFragmentTexture:image atIndex:0];
	[t_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
						  indexCount:t_command.index_count
						   indexType:MTLIndexTypeUInt32
						 indexBuffer:t_indices
				   indexBufferOffset:t_command.index_offset * sizeof(u32)];
}
}

auto create_native_render_backend() -> std::unique_ptr<RenderBackend>
{
	return std::make_unique<MetalBackend>();
}

auto native_render_backend_name() -> std::string_view
{
	return "Metal";
}
