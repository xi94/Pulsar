#include "render/render_backend.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>

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
	"ps_solid", "ps_textured", "ps_banner_glow", "ps_color_picker", "ps_shadow", "ps_outline_countdown", "ps_backdrop", "ps_backdrop_plain", "ps_backdrop_blur",
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

	// The frame so far, then halved level by level. Each level has a second target the blur passes bounce through.
	id<MTLRenderPipelineState> downsample_pipeline = nil;
	id<MTLRenderPipelineState> blur_pipeline       = nil;
	id<MTLTexture>             capture             = nil;
	id<MTLTexture>             levels[K_BLUR_LEVEL_COUNT][2]{};
	bool                       blur_ready = false;

	~MetalBackend() override;

	[[nodiscard]] auto init(const os::Window* t_window) -> bool override;
	auto resize(u32 t_physical_width, u32 t_physical_height) -> void override;
	auto render(const RenderFrame& t_frame) -> void override;

	[[nodiscard]] auto supports_backdrop_blur() const -> bool override
	{
		return blur_ready;
	}

	[[nodiscard]] auto create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool override;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void override;
	auto destroy_texture(u32 t_slot) -> void override;

	[[nodiscard]] auto create_pipelines() -> bool;
	auto create_sampler() -> void;
	auto size_blur_targets(u32 t_width, u32 t_height) -> void;
	auto encode_blur(id<MTLCommandBuffer> t_commands, BlurPlan t_plan) const -> void;
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
	size_blur_targets(t_physical_width, t_physical_height);
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

		const Color                        clear     = t_frame.clear_color;
		const DrawList*                    draw_list = t_frame.draw_list;
		const std::span<const DrawCommand> list      = draw_list->commands();
		const ViewportConstants            viewport  = viewport_constants(t_frame);
		FrameGeometry*                     geometry  = &frames[frame_index];
		id<MTLCommandBuffer>               commands  = [queue commandBuffer];

		if (!list.empty()) {
			upload_geometry(geometry, draw_list);
		}

		// Each pass runs up to the next glass that needs a fresh blur of what is drawn before it, or that blurs unlike the glass before it.
		// That pass resolves into the capture instead of the screen, the blur runs, and the next pass carries on from the multisampled
		// frame it left behind.
		usize first       = 0;
		bool  first_clear = true;

		while (true) {
			usize end          = first;
			bool  ends_in_blur = false;

			for (; end < list.size(); end += 1) {
				const bool glass = list[end].shader == ShaderKind::BACKDROP_BLUR;

				const bool follows_glass = end > first && list[end - 1].shader == ShaderKind::BACKDROP_BLUR;

				if (glass && blur_ready && end > first && (!follows_glass || blur_plan(t_frame, list[end - 1]) != blur_plan(t_frame, list[end]))) {
					ends_in_blur = true;
					break;
				}
			}

			MTLRenderPassDescriptor* pass           = [MTLRenderPassDescriptor renderPassDescriptor];
			pass.colorAttachments[0].texture        = msaa_target;
			pass.colorAttachments[0].resolveTexture = ends_in_blur ? capture : drawable.texture;
			pass.colorAttachments[0].loadAction     = first_clear ? MTLLoadActionClear : MTLLoadActionLoad;
			pass.colorAttachments[0].storeAction    = ends_in_blur ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionMultisampleResolve;
			pass.colorAttachments[0].clearColor     = MTLClearColorMake(clear.r / 255.0, clear.g / 255.0, clear.b / 255.0, clear.a / 255.0);

			id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];

			if (end > first) {
				[encoder setVertexBuffer:geometry->vertices offset:0 atIndex:K_VERTEX_BUFFER_INDEX];
				[encoder setVertexBytes:&viewport length:sizeof(viewport) atIndex:K_CONSTANTS_BUFFER_INDEX];
				[encoder setFragmentSamplerState:sampler atIndex:0];

				for (usize i = first; i < end; i += 1) {
					if (list[i].shader == ShaderKind::BACKDROP_BLUR && !blur_ready) continue;

					draw_command(encoder, t_frame, list[i], geometry->indices);
				}
			}

			[encoder endEncoding];

			if (!ends_in_blur) break;

			encode_blur(commands, blur_plan(t_frame, list[end]));
			first       = end;
			first_clear = false;
		}

		[commands presentDrawable:drawable];

		dispatch_semaphore_t semaphore = frames_available;
		[commands addCompletedHandler:^(id<MTLCommandBuffer>) {
			dispatch_semaphore_signal(semaphore);
		}];
		[commands commit];

		frame_index = (frame_index + 1) % K_FRAMES_IN_FLIGHT;
	}
}

auto MetalBackend::size_blur_targets(u32 t_width, u32 t_height) -> void
{
	const auto target = [this](u32 t_target_width, u32 t_target_height) {
		MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:K_PIXEL_FORMAT
																							  width:std::max<u32>(t_target_width, 1)
																							 height:std::max<u32>(t_target_height, 1)
																						  mipmapped:NO];
		descriptor.usage                 = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		descriptor.storageMode           = MTLStorageModePrivate;

		return [device newTextureWithDescriptor:descriptor];
	};

	capture    = target(t_width, t_height);
	blur_ready = downsample_pipeline != nil && blur_pipeline != nil && capture != nil;

	for (u32 level = 0; level < K_BLUR_LEVEL_COUNT; level += 1) {
		for (id<MTLTexture>& texture : levels[level]) {
			texture    = target(blur_level_extent(t_width, level), blur_level_extent(t_height, level));
			blur_ready = blur_ready && texture != nil;
		}
	}
}

// Halves the captured frame down to the plan's level and blurs it there for glass to sample.
auto MetalBackend::encode_blur(id<MTLCommandBuffer> t_commands, BlurPlan t_plan) const -> void
{
	const auto pass = [&](id<MTLTexture> t_target, id<MTLTexture> t_source, id<MTLRenderPipelineState> t_pipeline, float t_step_x, float t_step_y) {
		MTLRenderPassDescriptor* descriptor        = [MTLRenderPassDescriptor renderPassDescriptor];
		descriptor.colorAttachments[0].texture     = t_target;
		descriptor.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
		descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

		const BlurConstants         constants{.step_x = t_step_x, .step_y = t_step_y};
		id<MTLRenderCommandEncoder> encoder = [t_commands renderCommandEncoderWithDescriptor:descriptor];

		[encoder setRenderPipelineState:t_pipeline];
		[encoder setFragmentTexture:t_source atIndex:0];
		[encoder setFragmentSamplerState:sampler atIndex:0];
		[encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:K_CONSTANTS_BUFFER_INDEX];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		[encoder endEncoding];
	};

	id<MTLTexture> source = capture;

	for (u32 level = 0; level <= t_plan.level; level += 1) {
		pass(levels[level][0], source, downsample_pipeline, 1.0f / static_cast<float>(source.width), 1.0f / static_cast<float>(source.height));
		source = levels[level][0];
	}

	id<MTLTexture> blurred = levels[t_plan.level][0];
	id<MTLTexture> scratch = levels[t_plan.level][1];
	const float    step_x  = t_plan.step / static_cast<float>(blurred.width);
	const float    step_y  = t_plan.step / static_cast<float>(blurred.height);

	for (u32 i = 0; i < t_plan.iterations; i += 1) {
		pass(scratch, blurred, blur_pipeline, step_x, 0.0f);
		pass(blurred, scratch, blur_pipeline, 0.0f, step_y);
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

// A blit queued behind the frames in flight lets them finish reading the old pixels, so the CPU never waits for the GPU here.
auto MetalBackend::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	if (t_width == 0 || t_height == 0) return;

	@autoreleasepool {
		const NSUInteger row_bytes = static_cast<NSUInteger>(t_width) * 4;
		const NSUInteger bytes     = row_bytes * t_height;

		id<MTLBuffer>             staging  = [device newBufferWithBytes:t_rgba_pixels length:bytes options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer>      commands = [queue commandBuffer];
		id<MTLBlitCommandEncoder> blit     = [commands blitCommandEncoder];

		[blit copyFromBuffer:staging
				   sourceOffset:0
			  sourceBytesPerRow:row_bytes
			sourceBytesPerImage:bytes
					 sourceSize:MTLSizeMake(t_width, t_height, 1)
					  toTexture:textures[t_slot]
			   destinationSlice:0
			   destinationLevel:0
			  destinationOrigin:MTLOriginMake(t_x, t_y, 0)];
		[blit endEncoding];
		[commands commit];
	}
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

	MTLRenderPipelineDescriptor* passes    = [[MTLRenderPipelineDescriptor alloc] init];
	passes.vertexFunction                  = [library newFunctionWithName:@"vs_fullscreen"];
	passes.colorAttachments[0].pixelFormat = K_PIXEL_FORMAT;

	passes.fragmentFunction = [library newFunctionWithName:@"ps_downsample"];
	downsample_pipeline     = [device newRenderPipelineStateWithDescriptor:passes error:&error];
	passes.fragmentFunction = [library newFunctionWithName:@"ps_blur"];
	blur_pipeline           = [device newRenderPipelineStateWithDescriptor:passes error:&error];

	if (downsample_pipeline == nil || blur_pipeline == nil) {
		debug_log::write(K_LOG_CATEGORY, "the glass blur pipelines failed - glass draws without blur");
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

	if (t_command.shader == ShaderKind::TEXTURED) {
		if (!t_command.texture->is_valid()) return;

		image = textures[t_command.texture->slot()];
	} else if (t_command.shader == ShaderKind::BACKDROP_BLUR) {
		image = levels[blur_plan(t_frame, t_command).level][0];
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
