/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "nativewindow.hh"

template<class T> class TRect;
typedef TRect<int> Rect;
class Surface;


// How the presented frame is filtered when the window is larger than it.
enum VideoScaleMode {
	VIDEO_SCALE_NEAREST,
	VIDEO_SCALE_LINEAR,
	VIDEO_SCALE_PIXELART,
};


// While the tactical map is presented beneath the frame, frame pixels of this color leave a
// hole for it to show through.
constexpr unsigned short VIDEO_TRANSPARENT_PIXEL = 0xF81F;


// Where the game's frame lands inside the window. The destination keeps the resolution's
// aspect ratio, so it is centered and the window may show bars on two of its sides. The
// frame matches the resolution unless the interface is scaled, and is stretched over the
// destination either way. Drawable dimensions and the destination rectangle are measured
// in physical pixels.
struct VideoScaleInfo
{
	int GameWidth;
	int GameHeight;
	int ResolutionWidth;
	int ResolutionHeight;
	int DrawableWidth;
	int DrawableHeight;
	int DestX;
	int DestY;
	int DestWidth;
	int DestHeight;
	float ScaleX;
	float ScaleY;
};


// The resolution the tactical map is drawn at. VideoModeWidth and VideoModeHeight hold the
// frame the interface is drawn in, which differs only while the interface is scaled.
extern int VideoResolutionWidth;
extern int VideoResolutionHeight;


bool Video_Init(NativeWindow const & window, int drawablewidth, int drawableheight, int refreshrate);
void Video_Shutdown(void);

bool Video_Set_Mode(int width, int height, int resolutionwidth, int resolutionheight);
void Video_On_Resize(int drawablewidth, int drawableheight);
void Video_Set_Refresh_Rate(int refreshrate);

void Video_Show_World(Surface & surface, Rect const & source, Rect const & dest, int shiftx, int shifty);
void Video_Hide_World(void);

void Video_Mark_Dirty(void);
void Video_Present(void);
void Video_Present_If_Dirty(void);

VideoScaleInfo const & Video_Get_Scale_Info(void);

int * EnumDisplayModes(int minwidth, int minheight, int maxwidth, int maxheight);
