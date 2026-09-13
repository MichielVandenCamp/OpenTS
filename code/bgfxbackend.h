/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The renderer's private interface. Only bgfxbackend.cpp includes bgfx, so no bgfx type
// appears here and no other translation unit needs the library's headers or its build
// settings. video.cpp is the only caller.

#pragma once

#include "nativewindow.hh"


enum BackendRenderer {
	BACKEND_RENDERER_AUTO,
	BACKEND_RENDERER_D3D11,
	BACKEND_RENDERER_D3D12,
	BACKEND_RENDERER_VULKAN,
	BACKEND_RENDERER_OPENGL,
};


enum BackendScaleMode {
	BACKEND_SCALE_NEAREST,
	BACKEND_SCALE_LINEAR,
	BACKEND_SCALE_PIXELART,
};


// Where a picture is drawn: part of the picture in its own pixels, the window rectangle it
// is stretched over, and the window rectangle nothing is drawn outside of. A clip rectangle
// with no area clips nothing. Window rectangles are in physical pixels.
struct BackendPlacement
{
	int SourceX;
	int SourceY;
	int SourceWidth;
	int SourceHeight;
	int DestX;
	int DestY;
	int DestWidth;
	int DestHeight;
	int ClipX;
	int ClipY;
	int ClipWidth;
	int ClipHeight;
};


// Drawable sizes are physical pixel dimensions supplied by the application shell.
bool Backend_Init(NativeWindow const & window, int drawablewidth, int drawableheight, BackendRenderer renderer, bool vsync);
void Backend_Shutdown(void);

bool Backend_Set_Frame_Size(int width, int height, int transparentpixel);
bool Backend_Set_World_Frame(void const * pixels, int pitch, int width, int height);
void Backend_On_Resize(int drawablewidth, int drawableheight);

// Uploads the frame and presents it, over the last world picture when a placement for it is
// given. The pixels are 16 bit 565 and stay owned by the caller; they are consumed before
// this returns.
void Backend_Present(void const * pixels, int pitch, int destx, int desty, int destwidth, int destheight, BackendScaleMode mode, BackendPlacement const * world);

char const * Backend_Renderer_Name(void);
