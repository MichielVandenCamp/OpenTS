/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The bgfx side of the presenter. This is the only translation unit that includes bgfx,
// which keeps the library's headers and build settings away from the rest of the engine.

#include "bgfxbackend.h"

#include "dbgprint.h"
#include "except.h"

#include <bx/allocator.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

#include <vs_ocornut_imgui.bin.h>
#include <fs_ocornut_imgui.bin.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>


static const bgfx::EmbeddedShader _EmbeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER_END()
};


// The views that magnify the world picture and the frame when the pixel art filter needs an
// intermediate target, and the one that draws onto the window. Views render in ascending
// order, so the magnify passes must carry the lower ids for the present pass to sample their
// output from this frame rather than the last one.
static const bgfx::ViewId VIEW_PRESCALE_WORLD = 0;
static const bgfx::ViewId VIEW_PRESCALE = 1;
static const bgfx::ViewId VIEW_PRESENT = 2;

// A frame with transparent pixels is blended over the world picture. Its pixels are either
// opaque or wholly transparent black, so they already count as premultiplied.
static const uint64_t STATE_OPAQUE = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
static const uint64_t STATE_BLENDED = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);


// A 16 bit 565 picture held in a texture. When the hardware cannot sample that format, or
// the picture has a transparent pixel, it is widened to 32 bits on the way in instead.
struct BackendPicture
{
	bgfx::TextureHandle Texture;
	int Width;
	int Height;
	bool Is565;
	int TransparentPixel;
	unsigned int * ConvertBuffer;
};

// The intermediate target the pixel art filter magnifies a picture through.
struct PrescaleTarget
{
	bgfx::FrameBufferHandle Handle;
	int Width;
	int Height;
};


static bool _Initialized = false;

static BackendPicture _Frame = { BGFX_INVALID_HANDLE, 0, 0, false, -1, NULL };
static BackendPicture _World = { BGFX_INVALID_HANDLE, 0, 0, false, -1, NULL };
static PrescaleTarget _FramePrescale = { BGFX_INVALID_HANDLE, 0, 0 };
static PrescaleTarget _WorldPrescale = { BGFX_INVALID_HANDLE, 0, 0 };

static bgfx::ProgramHandle _Program = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle _TextureSampler = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout _VertexLayout;

static int _DrawableWidth = 0;
static int _DrawableHeight = 0;
static unsigned int _ResetFlags = BGFX_RESET_FLIP_AFTER_RENDER;

static unsigned int _ConvertTable[65536];


struct BackendVertex
{
	float X;
	float Y;
	float U;
	float V;
	unsigned int Color;
};


// bgfx reports lost devices and shader failures through this rather than a return code,
// so the engine would otherwise present to a black window with no explanation.
class BackendCallback : public bgfx::CallbackI
{
	public:
		virtual ~BackendCallback(void) override {}

		virtual void fatal(const char * filepath, uint16_t line, bgfx::Fatal::Enum code, const char * str) override
		{
			// A debug check is the library's own assertion, not a renderer failure. The ones it
			// runs while shutting down compare reference counts on interfaces that an overlay
			// or the Direct3D debug layer is free to hold, so ending the process over one would
			// report somebody else's reference as a crash.
			if (code == bgfx::Fatal::DebugCheck) {
				DebugString("Renderer check failed at %s(%u): %s\n",
							filepath != NULL ? filepath : "", (unsigned)line, str != NULL ? str : "");
				return;
			}

			Fatal("Renderer error %d at %s(%u): %s", (int)code,
						filepath != NULL ? filepath : "", (unsigned)line, str != NULL ? str : "");
		}

		virtual void traceVargs(const char * filepath, uint16_t line, const char * format, va_list argList) override
		{
			char message[1024];
			vsnprintf(message, sizeof(message), format, argList);
			OutputDebugString(message);
		}

		virtual void profilerBegin(const char *, uint32_t, const char *, uint16_t) override {}
		virtual void profilerBeginLiteral(const char *, uint32_t, const char *, uint16_t) override {}
		virtual void profilerEnd(void) override {}
		virtual uint32_t cacheReadSize(uint64_t) override { return(0); }
		virtual bool cacheRead(uint64_t, void *, uint32_t) override { return(false); }
		virtual void cacheWrite(uint64_t, const void *, uint32_t) override {}
		virtual void screenShot(const char *, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, const void *, uint32_t, bool) override {}
		virtual void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
		virtual void captureEnd(void) override {}
		virtual void captureFrame(const void *, uint32_t) override {}
};

static BackendCallback _Callback;


// bgfx contains cache-line-aligned render records but requests their backing arrays with
// the allocator's default alignment. The Win32 CRT only guarantees eight-byte alignment,
// which is insufficient when clang-cl copies those records with aligned SSE instructions.
class BackendAllocator : public bx::AllocatorI
{
	public:
		virtual ~BackendAllocator(void) override {}

		virtual void * realloc(void * ptr, size_t size, size_t alignment, const char *, uint32_t) override
		{
			if (size == 0) {
				_aligned_free(ptr);
				return(NULL);
			}

			const size_t cachelinealignment = BX_CACHE_LINE_SIZE;
			alignment = std::max(alignment, cachelinealignment);
			return(_aligned_realloc(ptr, size, alignment));
		}
};

static BackendAllocator _Allocator;


/// <summary>
/// Builds the table that widens a 565 pixel to the 32 bit color the fallback path uploads.
/// </summary>
static void Build_Convert_Table(void)
{
	for (int pixel = 0; pixel < 65536; pixel++) {
		unsigned int red = (unsigned int)(((pixel >> 11) & 0x1F) * 255 / 31);
		unsigned int green = (unsigned int)(((pixel >> 5) & 0x3F) * 255 / 63);
		unsigned int blue = (unsigned int)((pixel & 0x1F) * 255 / 31);

		_ConvertTable[pixel] = 0xFF000000 | (red << 16) | (green << 8) | blue;
	}
}


/// <summary>
/// Submits one textured rectangle covering the given destination.
/// </summary>
static void Submit_Quad(bgfx::ViewId view, bgfx::TextureHandle texture, float x, float y, float width, float height, float u0, float v0, float u1, float v1, unsigned int samplerflags, uint64_t state)
{
	bgfx::TransientVertexBuffer buffer;

	if (bgfx::getAvailTransientVertexBuffer(6, _VertexLayout) < 6) {
		return;
	}

	bgfx::allocTransientVertexBuffer(&buffer, 6, _VertexLayout);

	BackendVertex * vertex = (BackendVertex *)buffer.data;
	const unsigned int white = 0xFFFFFFFF;

	vertex[0] = { x, y, u0, v0, white };
	vertex[1] = { x + width, y, u1, v0, white };
	vertex[2] = { x + width, y + height, u1, v1, white };
	vertex[3] = { x, y, u0, v0, white };
	vertex[4] = { x + width, y + height, u1, v1, white };
	vertex[5] = { x, y + height, u0, v1, white };

	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setTexture(0, _TextureSampler, texture, samplerflags);
	bgfx::setState(state);
	bgfx::submit(view, _Program);
}


/// <summary>
/// Builds an orthographic projection over a target measured in pixels, with the origin in
/// its top left corner.
/// </summary>
static void Build_Ortho_Projection(float * result, int width, int height)
{
	const float depthnear = 0.0f;
	const float depthfar = 1000.0f;
	const bool homogeneous = bgfx::getCaps()->homogeneousDepth;

	memset(result, 0, sizeof(float) * 16);

	result[0] = 2.0f / (float)width;
	result[5] = -2.0f / (float)height;
	result[10] = homogeneous ? 2.0f / (depthfar - depthnear) : 1.0f / (depthfar - depthnear);
	result[12] = -1.0f;
	result[13] = 1.0f;
	result[14] = homogeneous ? -(depthfar + depthnear) / (depthfar - depthnear) : -depthnear / (depthfar - depthnear);
	result[15] = 1.0f;
}


/// <summary>
/// Sets a view to draw into a target of the given size using pixel coordinates.
/// </summary>
static void Set_View_Transform(bgfx::ViewId view, int width, int height)
{
	float projection[16];
	bgfx::setViewRect(view, 0, 0, (uint16_t)width, (uint16_t)height);
	Build_Ortho_Projection(projection, width, height);
	bgfx::setViewTransform(view, NULL, projection);
}


/// <summary>
/// Discards an intermediate target the pixel art filter magnifies through.
/// </summary>
static void Destroy_Prescale_Target(PrescaleTarget & target)
{
	if (bgfx::isValid(target.Handle)) {
		bgfx::destroy(target.Handle);
		target.Handle = BGFX_INVALID_HANDLE;
	}
	target.Width = 0;
	target.Height = 0;
}


/// <summary>
/// Makes sure an intermediate target for the pixel art filter has the requested size.
/// </summary>
/// <returns>bool; Is a target of that size ready to render into?</returns>
static bool Ensure_Prescale_Target(PrescaleTarget & target, int width, int height)
{
	if (bgfx::isValid(target.Handle) && target.Width == width && target.Height == height) {
		return(true);
	}

	Destroy_Prescale_Target(target);

	const bgfx::Caps * caps = bgfx::getCaps();
	if (width <= 0 || height <= 0 || width > caps->limits.maxTextureSize || height > caps->limits.maxTextureSize) {
		return(false);
	}

	target.Handle = bgfx::createFrameBuffer((uint16_t)width, (uint16_t)height, bgfx::TextureFormat::BGRA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
	if (!bgfx::isValid(target.Handle)) {
		return(false);
	}

	target.Width = width;
	target.Height = height;
	return(true);
}


/// <summary>
/// Releases a picture's texture and the buffer it was widened in.
/// </summary>
static void Release_Picture(BackendPicture & picture)
{
	if (bgfx::isValid(picture.Texture)) {
		bgfx::destroy(picture.Texture);
		picture.Texture = BGFX_INVALID_HANDLE;
	}

	delete [] picture.ConvertBuffer;
	picture.ConvertBuffer = NULL;
	picture.Width = 0;
	picture.Height = 0;
}


/// <summary>
/// Makes sure a picture has a texture of the requested size, replacing any earlier one.
/// </summary>
/// <param name="transparentpixel">The 565 value that is drawn wholly transparent, or -1 for
/// an opaque picture.</param>
/// <returns>bool; Is a texture of that size ready to receive the picture?</returns>
static bool Prepare_Picture(BackendPicture & picture, int width, int height, int transparentpixel)
{
	if (bgfx::isValid(picture.Texture) && picture.Width == width && picture.Height == height && picture.TransparentPixel == transparentpixel) {
		return(true);
	}

	Release_Picture(picture);

	// bgfx names packed formats from their low bits up, so its B5G6R5 is the layout the
	// game already draws in. Emulated support would convert every upload on the way
	// through, which is what the fallback below does more cheaply.
	const bgfx::Caps * caps = bgfx::getCaps();
	picture.Is565 = transparentpixel < 0 && (caps->formats[bgfx::TextureFormat::B5G6R5] & BGFX_CAPS_FORMAT_TEXTURE_2D) != 0;
	picture.TransparentPixel = transparentpixel;

	picture.Texture = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, picture.Is565 ? bgfx::TextureFormat::B5G6R5 : bgfx::TextureFormat::BGRA8);
	if (!bgfx::isValid(picture.Texture)) {
		return(false);
	}

	if (!picture.Is565) {
		if (_ConvertTable[0xFFFF] == 0) {
			Build_Convert_Table();
		}
		picture.ConvertBuffer = new unsigned int[width * height];
	}

	picture.Width = width;
	picture.Height = height;
	return(true);
}


/// <summary>
/// Copies a 565 picture into its texture, widening it on the way when the texture is not 565.
/// </summary>
static void Upload_Picture(BackendPicture & picture, void const * pixels, int pitch)
{
	if (picture.Is565) {
		bgfx::updateTexture2D(picture.Texture, 0, 0, 0, 0, (uint16_t)picture.Width, (uint16_t)picture.Height, bgfx::copy(pixels, (uint32_t)(picture.Height * pitch)), (uint16_t)pitch);
		return;
	}

	if (picture.ConvertBuffer == NULL) {
		return;
	}

	for (int y = 0; y < picture.Height; y++) {
		unsigned short const * source = (unsigned short const *)((char const *)pixels + y * pitch);
		unsigned int * dest = picture.ConvertBuffer + y * picture.Width;
		if (picture.TransparentPixel < 0) {
			for (int x = 0; x < picture.Width; x++) {
				dest[x] = _ConvertTable[source[x]];
			}
		} else {
			for (int x = 0; x < picture.Width; x++) {
				dest[x] = (source[x] == picture.TransparentPixel) ? 0 : _ConvertTable[source[x]];
			}
		}
	}
	bgfx::updateTexture2D(picture.Texture, 0, 0, 0, 0, (uint16_t)picture.Width, (uint16_t)picture.Height, bgfx::copy(picture.ConvertBuffer, (uint32_t)(picture.Width * picture.Height * 4)), (uint16_t)(picture.Width * 4));
}


/// <summary>
/// Draws part of a picture onto the window with the requested filter.
/// The pixel art filter keeps whole pixels whole. An exact multiple needs nothing but point
/// sampling; anything else is magnified to the next whole multiple with point sampling and
/// then shrunk to the window smoothly, which keeps edges sharp without the uneven pixel
/// sizes that point sampling alone would give.
/// </summary>
static void Submit_Picture(BackendPicture const & picture, bgfx::ViewId prescaleview, PrescaleTarget & target, BackendPlacement const & placement, BackendScaleMode mode, uint64_t state)
{
	if (placement.SourceWidth <= 0 || placement.SourceHeight <= 0 || picture.Width <= 0 || picture.Height <= 0) {
		return;
	}

	bgfx::TextureHandle source = picture.Texture;
	unsigned int samplerflags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
	bool flipv = false;

	if (mode == BACKEND_SCALE_NEAREST) {
		samplerflags |= BGFX_SAMPLER_POINT;
	}

	if (mode == BACKEND_SCALE_PIXELART && placement.DestWidth > placement.SourceWidth && placement.DestHeight > placement.SourceHeight) {
		if ((placement.DestWidth % placement.SourceWidth) == 0 && (placement.DestHeight % placement.SourceHeight) == 0) {
			samplerflags |= BGFX_SAMPLER_POINT;
		} else {
			int scale = (placement.DestWidth + placement.SourceWidth - 1) / placement.SourceWidth;
			int scaley = (placement.DestHeight + placement.SourceHeight - 1) / placement.SourceHeight;
			if (scaley > scale) {
				scale = scaley;
			}

			if (Ensure_Prescale_Target(target, picture.Width * scale, picture.Height * scale)) {
				bgfx::setViewFrameBuffer(prescaleview, target.Handle);
				bgfx::setViewClear(prescaleview, BGFX_CLEAR_COLOR, 0x000000FF);
				Set_View_Transform(prescaleview, target.Width, target.Height);
				Submit_Quad(prescaleview, picture.Texture, 0.0f, 0.0f, (float)target.Width, (float)target.Height, 0.0f, 0.0f, 1.0f, 1.0f, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT, STATE_OPAQUE);
				source = bgfx::getTexture(target.Handle);
				flipv = bgfx::getCaps()->originBottomLeft;
			}
		}
	}

	float u0 = (float)placement.SourceX / (float)picture.Width;
	float u1 = (float)(placement.SourceX + placement.SourceWidth) / (float)picture.Width;
	float v0 = (float)placement.SourceY / (float)picture.Height;
	float v1 = (float)(placement.SourceY + placement.SourceHeight) / (float)picture.Height;

	if (flipv) {
		v0 = 1.0f - v0;
		v1 = 1.0f - v1;
	}

	if (placement.ClipWidth > 0 && placement.ClipHeight > 0) {
		bgfx::setScissor((uint16_t)placement.ClipX, (uint16_t)placement.ClipY, (uint16_t)placement.ClipWidth, (uint16_t)placement.ClipHeight);
	}

	Submit_Quad(VIEW_PRESENT, source, (float)placement.DestX, (float)placement.DestY, (float)placement.DestWidth, (float)placement.DestHeight, u0, v0, u1, v1, samplerflags, state);
}


/// <summary>
/// Starts the renderer on an existing window.
/// </summary>
/// <param name="window">The window the frame is presented into.</param>
/// <param name="drawablewidth">The drawable area's width in physical pixels.</param>
/// <param name="drawableheight">The drawable area's height in physical pixels.</param>
/// <param name="renderer">Which graphics API to ask for, or auto to let bgfx decide.</param>
/// <param name="vsync">Should presents wait for the display's refresh?</param>
/// <returns>bool; Did the renderer start?</returns>
bool Backend_Init(NativeWindow const & window, int drawablewidth, int drawableheight, BackendRenderer renderer, bool vsync)
{
	if (_Initialized) {
		return(true);
	}

	// Presents happen at whatever depth the engine has reached, including from inside a
	// dialog's paint handler, so the renderer has to run on this thread. Calling
	// renderFrame before init is what selects that.
	bgfx::renderFrame();

	_DrawableWidth = drawablewidth;
	_DrawableHeight = drawableheight;
	_ResetFlags = BGFX_RESET_FLIP_AFTER_RENDER | (vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE);

	bgfx::Init init;
	init.platformData.ndt = window.Display;
	init.platformData.nwh = window.Handle;
	init.platformData.type = window.Type == NATIVE_WINDOW_WAYLAND
		? bgfx::NativeWindowHandleType::Wayland
		: bgfx::NativeWindowHandleType::Default;
	init.resolution.width = (uint32_t)drawablewidth;
	init.resolution.height = (uint32_t)drawableheight;
	init.resolution.reset = _ResetFlags;
	init.callback = &_Callback;
	init.allocator = &_Allocator;

	switch (renderer) {
		case BACKEND_RENDERER_D3D11:
			init.type = bgfx::RendererType::Direct3D11;
			break;

		case BACKEND_RENDERER_D3D12:
			init.type = bgfx::RendererType::Direct3D12;
			break;

		case BACKEND_RENDERER_VULKAN:
			init.type = bgfx::RendererType::Vulkan;
			break;

		case BACKEND_RENDERER_OPENGL:
			init.type = bgfx::RendererType::OpenGL;
			break;

		default:
			init.type = bgfx::RendererType::Count;
			break;
	}

	if (!bgfx::init(init)) {
		return(false);
	}

	_VertexLayout.begin()
		.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vertexshader = bgfx::createEmbeddedShader(_EmbeddedShaders, type, "vs_ocornut_imgui");
	bgfx::ShaderHandle fragmentshader = bgfx::createEmbeddedShader(_EmbeddedShaders, type, "fs_ocornut_imgui");

	if (!bgfx::isValid(vertexshader) || !bgfx::isValid(fragmentshader)) {
		bgfx::shutdown();
		return(false);
	}

	_Program = bgfx::createProgram(vertexshader, fragmentshader, true);
	_TextureSampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

	if (!bgfx::isValid(_Program) || !bgfx::isValid(_TextureSampler)) {
		bgfx::shutdown();
		return(false);
	}

	// The frame is blended over the world picture, so the two must reach the window in the
	// order they are submitted rather than whatever order bgfx finds cheapest.
	bgfx::setViewMode(VIEW_PRESENT, bgfx::ViewMode::Sequential);

	_Initialized = true;
	return(true);
}


/// <summary>
/// Shuts the renderer down and releases everything it created.
/// </summary>
void Backend_Shutdown(void)
{
	if (!_Initialized) {
		return;
	}

	Destroy_Prescale_Target(_FramePrescale);
	Destroy_Prescale_Target(_WorldPrescale);
	Release_Picture(_Frame);
	Release_Picture(_World);

	if (bgfx::isValid(_TextureSampler)) {
		bgfx::destroy(_TextureSampler);
		_TextureSampler = BGFX_INVALID_HANDLE;
	}
	if (bgfx::isValid(_Program)) {
		bgfx::destroy(_Program);
		_Program = BGFX_INVALID_HANDLE;
	}

	bgfx::shutdown();

	_Initialized = false;
}


/// <summary>
/// Points the renderer at a frame of the given size, replacing any earlier one.
/// </summary>
/// <param name="transparentpixel">The 565 value that lets the world picture show through,
/// or -1 for a frame that covers it completely.</param>
/// <returns>bool; Is a texture of that size ready to receive frames?</returns>
bool Backend_Set_Frame_Size(int width, int height, int transparentpixel)
{
	if (!_Initialized || width <= 0 || height <= 0) {
		return(false);
	}

	return(Prepare_Picture(_Frame, width, height, transparentpixel));
}


/// <summary>
/// Uploads the picture that is drawn beneath the frame when a present asks for it.
/// </summary>
/// <param name="pixels">The picture's top left pixel, in 16 bit 565.</param>
/// <param name="pitch">The bytes between one row of the picture and the next.</param>
/// <param name="width">The picture's width.</param>
/// <param name="height">The picture's height.</param>
/// <returns>bool; Is the picture ready to be drawn?</returns>
bool Backend_Set_World_Frame(void const * pixels, int pitch, int width, int height)
{
	if (!_Initialized || pixels == NULL || width <= 0 || height <= 0) {
		return(false);
	}

	if (!Prepare_Picture(_World, width, height, -1)) {
		return(false);
	}

	Upload_Picture(_World, pixels, pitch);
	return(true);
}


/// <summary>
/// Tells the renderer the drawable area changed size.
/// </summary>
void Backend_On_Resize(int drawablewidth, int drawableheight)
{
	if (!_Initialized || drawablewidth <= 0 || drawableheight <= 0) {
		return;
	}

	if (_DrawableWidth == drawablewidth && _DrawableHeight == drawableheight) {
		return;
	}

	_DrawableWidth = drawablewidth;
	_DrawableHeight = drawableheight;
	bgfx::reset((uint32_t)drawablewidth, (uint32_t)drawableheight, _ResetFlags);
}


/// <summary>
/// Uploads the frame and puts it on the screen.
/// </summary>
/// <param name="pixels">The frame's top left pixel, in 16 bit 565.</param>
/// <param name="pitch">The bytes between one row of that frame and the next.</param>
/// <param name="destx">Where the left edge of the frame lands in the window.</param>
/// <param name="desty">Where the top edge of the frame lands in the window.</param>
/// <param name="destwidth">How wide the frame is drawn.</param>
/// <param name="destheight">How tall the frame is drawn.</param>
/// <param name="mode">How the frame and the world picture are filtered when they are drawn
/// larger than they are.</param>
/// <param name="world">Where the last world picture is drawn beneath the frame, or NULL to
/// draw the frame alone.</param>
void Backend_Present(void const * pixels, int pitch, int destx, int desty, int destwidth, int destheight, BackendScaleMode mode, BackendPlacement const * world)
{
	if (!_Initialized || pixels == NULL || !bgfx::isValid(_Frame.Texture)) {
		return;
	}

	// A minimized window has no client area to present into.
	if (_DrawableWidth <= 0 || _DrawableHeight <= 0) {
		return;
	}

	Upload_Picture(_Frame, pixels, pitch);

	// Clearing the whole window is what paints the bars beside a frame that does not
	// share the window's shape.
	bgfx::setViewFrameBuffer(VIEW_PRESENT, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(VIEW_PRESENT, BGFX_CLEAR_COLOR, 0x000000FF);
	Set_View_Transform(VIEW_PRESENT, _DrawableWidth, _DrawableHeight);

	if (world != NULL && bgfx::isValid(_World.Texture)) {
		Submit_Picture(_World, VIEW_PRESCALE_WORLD, _WorldPrescale, *world, mode, STATE_OPAQUE);
	}

	BackendPlacement frame = { 0, 0, _Frame.Width, _Frame.Height, destx, desty, destwidth, destheight, 0, 0, 0, 0 };
	Submit_Picture(_Frame, VIEW_PRESCALE, _FramePrescale, frame, mode, _Frame.TransparentPixel < 0 ? STATE_OPAQUE : STATE_BLENDED);

	bgfx::frame();
}


/// <summary>
/// Names the graphics API the renderer settled on.
/// </summary>
char const * Backend_Renderer_Name(void)
{
	if (!_Initialized) {
		return("none");
	}
	return(bgfx::getRendererName(bgfx::getRendererType()));
}
