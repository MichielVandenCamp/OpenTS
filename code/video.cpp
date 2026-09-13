/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The engine's side of the presenter. The game draws its frame into the visible surface
// as it always has; this decides when that frame reaches the screen and where in the
// window it lands, and hands it to the renderer behind video.h. While the tactical map is
// drawn at another scale than the interface, a picture of the map is handed over as well
// and drawn beneath it.

#include "always.h"

#include "video.h"

#include "_surface.h"
#include "bgfxbackend.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "globals.h"
#include "goptions.h"
#include "misc.h"
#include "rect.h"
#include "surface.h"
#include "uiscale.h"
#include "wincursor.h"

#include <cstdlib>
#include <cstring>


/*
 * The size of the frame the game renders into. It is not tied to the window, which may be
 * any size, nor to the desktop, whose mode the game no longer changes.
 */
int VideoModeWidth = 0;
int VideoModeHeight = 0;

/*
 * The resolution, whose shape the picture is fitted to the window by. The frame has the same
 * size unless the interface is scaled, in which case the frame is stretched over the area the
 * resolution fills.
 */
int VideoResolutionWidth = 0;
int VideoResolutionHeight = 0;

/*
 * How many pixels of the tactical map the screen is tall. The map is drawn on a frame of this
 * height with the resolution's shape, which is stretched over the same area as the frame the
 * interface is drawn in. Zero draws the map at the resolution.
 */
int VideoWorldHeight = 0;

/*
 * Is the game running in a framed, resizable window rather than in a borderless one
 * covering the whole screen? The display mode is never changed either way.
 */
bool WindowedMode = false;

static bool _Initialized = false;
static VideoScaleInfo _ScaleInfo;

// Set whenever the visible surface is written to, and cleared once that frame has been
// presented. A frame that is skipped for pacing stays marked, so the next present shows
// the newest content rather than a stale one.
static bool _FrameIsDirty = false;
static unsigned int _LastPresentTime = 0;
static unsigned int _PresentInterval = 16;

// Presents can nest, because a dialog repainting itself presents from inside the paint
// that the engine's own present provoked.
static bool _Presenting = false;

// The last picture of the tactical map handed over while the map is drawn at another scale
// than the interface. It is copied so that a present at any later moment shows a finished
// picture, and it reaches the renderer only when a present happens.
static unsigned short * _WorldPixels = NULL;
static int _WorldWidth = 0;
static int _WorldHeight = 0;
static bool _WorldVisible = false;
static bool _WorldChanged = false;
static Rect _WorldSource(0, 0, 0, 0);
static Rect _WorldDest(0, 0, 0, 0);
static int _WorldShiftX = 0;
static int _WorldShiftY = 0;


/// <summary>
/// Works out the shortest sensible gap between presents from the display's refresh rate.
/// </summary>
static void Update_Present_Interval(int refreshrate)
{
	if (refreshrate <= 1) {
		refreshrate = 60;
	}

	_PresentInterval = (unsigned int)(1000 / refreshrate);
	if (_PresentInterval < 3) {
		_PresentInterval = 3;
	}
	if (_PresentInterval > 100) {
		_PresentInterval = 100;
	}
}


/// <summary>
/// Works out the frame the tactical map is drawn in.
/// </summary>
/// <param name="height">How many pixels of the map the screen is tall, or zero or less for
/// the resolution itself.</param>
static void World_Frame_Size(int resolutionwidth, int resolutionheight, int height, int & framewidth, int & frameheight)
{
	if (height <= 0 || resolutionheight <= 0) {
		framewidth = resolutionwidth;
		frameheight = resolutionheight;
		return;
	}

	framewidth = Scale_Frame_Edge(resolutionwidth, resolutionheight, height);
	frameheight = height;
}


/// <summary>
/// Works out where the game's frame sits inside the window.
/// The picture keeps the resolution's shape, so it is grown by whichever of the two axes
/// runs out first and centered in what is left over. The frame is stretched over it.
/// </summary>
static void Update_Scale_Info(void)
{
	_ScaleInfo.GameWidth = VideoModeWidth;
	_ScaleInfo.GameHeight = VideoModeHeight;
	_ScaleInfo.ResolutionWidth = VideoResolutionWidth > 0 ? VideoResolutionWidth : VideoModeWidth;
	_ScaleInfo.ResolutionHeight = VideoResolutionHeight > 0 ? VideoResolutionHeight : VideoModeHeight;
	World_Frame_Size(_ScaleInfo.ResolutionWidth, _ScaleInfo.ResolutionHeight, VideoWorldHeight, _ScaleInfo.WorldWidth, _ScaleInfo.WorldHeight);

	if (_ScaleInfo.GameWidth <= 0 || _ScaleInfo.GameHeight <= 0 || _ScaleInfo.DrawableWidth <= 0 || _ScaleInfo.DrawableHeight <= 0) {
		_ScaleInfo.DestX = 0;
		_ScaleInfo.DestY = 0;
		_ScaleInfo.DestWidth = _ScaleInfo.DrawableWidth;
		_ScaleInfo.DestHeight = _ScaleInfo.DrawableHeight;
		_ScaleInfo.ScaleX = 1.0f;
		_ScaleInfo.ScaleY = 1.0f;
		return;
	}

	double scalex = (double)_ScaleInfo.DrawableWidth / (double)_ScaleInfo.ResolutionWidth;
	double scaley = (double)_ScaleInfo.DrawableHeight / (double)_ScaleInfo.ResolutionHeight;
	double scale = (scalex < scaley) ? scalex : scaley;

	if (Options.IntegerScaling && scale >= 1.0) {
		scale = (double)(int)scale;
	}

	_ScaleInfo.DestWidth = (int)((double)_ScaleInfo.ResolutionWidth * scale);
	_ScaleInfo.DestHeight = (int)((double)_ScaleInfo.ResolutionHeight * scale);
	_ScaleInfo.DestX = (_ScaleInfo.DrawableWidth - _ScaleInfo.DestWidth) / 2;
	_ScaleInfo.DestY = (_ScaleInfo.DrawableHeight - _ScaleInfo.DestHeight) / 2;
	_ScaleInfo.ScaleX = (float)((double)_ScaleInfo.DestWidth / (double)_ScaleInfo.GameWidth);
	_ScaleInfo.ScaleY = (float)((double)_ScaleInfo.DestHeight / (double)_ScaleInfo.GameHeight);
}


/// <summary>
/// Converts the configured filter into the one the renderer names.
/// </summary>
static BackendScaleMode Backend_Scale_Mode(void)
{
	switch (Options.ScaleMode) {
		case VIDEO_SCALE_LINEAR:
			return(BACKEND_SCALE_LINEAR);

		case VIDEO_SCALE_NEAREST:
			return(BACKEND_SCALE_NEAREST);

		default:
			return(BACKEND_SCALE_PIXELART);
	}
}


/// <summary>
/// Picks the frame pixel that leaves a hole for the tactical map.
/// </summary>
/// <returns>int; VIDEO_TRANSPARENT_PIXEL while the map is drawn at another scale than the
/// interface, or -1 while the map is drawn into the frame itself.</returns>
static int Frame_Transparent_Pixel(int width, int height, int resolutionwidth, int resolutionheight, int worldheight)
{
	if (resolutionwidth <= 0 || resolutionheight <= 0) {
		return(-1);
	}

	int worldframewidth = 0;
	int worldframeheight = 0;
	World_Frame_Size(resolutionwidth, resolutionheight, worldheight, worldframewidth, worldframeheight);

	if (worldframewidth == width && worldframeheight == height) {
		return(-1);
	}
	return(VIDEO_TRANSPARENT_PIXEL);
}


/// <summary>
/// Works out where the tactical map's picture is drawn in the window, handing the picture
/// to the renderer first if it has changed since the last present.
/// </summary>
/// <returns>bool; Is there a picture to draw?</returns>
static bool Place_World(BackendPlacement & placement)
{
	if (_WorldPixels == NULL || _WorldSource.Width <= 0 || _WorldSource.Height <= 0 || _ScaleInfo.GameWidth <= 0 || _ScaleInfo.GameHeight <= 0) {
		return(false);
	}

	if (_WorldChanged) {
		if (!Backend_Set_World_Frame(_WorldPixels, _WorldWidth * (int)sizeof(unsigned short), _WorldWidth, _WorldHeight)) {
			return(false);
		}
		_WorldChanged = false;
	}

	int left = _ScaleInfo.DestX + Scale_Frame_Edge(_WorldDest.X, _ScaleInfo.GameWidth, _ScaleInfo.DestWidth);
	int right = _ScaleInfo.DestX + Scale_Frame_Edge(_WorldDest.X + _WorldDest.Width, _ScaleInfo.GameWidth, _ScaleInfo.DestWidth);
	int top = _ScaleInfo.DestY + Scale_Frame_Edge(_WorldDest.Y, _ScaleInfo.GameHeight, _ScaleInfo.DestHeight);
	int bottom = _ScaleInfo.DestY + Scale_Frame_Edge(_WorldDest.Y + _WorldDest.Height, _ScaleInfo.GameHeight, _ScaleInfo.DestHeight);

	placement.SourceX = _WorldSource.X;
	placement.SourceY = _WorldSource.Y;
	placement.SourceWidth = _WorldSource.Width;
	placement.SourceHeight = _WorldSource.Height;
	placement.DestWidth = right - left;
	placement.DestHeight = bottom - top;
	placement.DestX = left + Scale_Frame_Edge(_WorldShiftX, _WorldSource.Width, placement.DestWidth);
	placement.DestY = top + Scale_Frame_Edge(_WorldShiftY, _WorldSource.Height, placement.DestHeight);
	placement.ClipX = left;
	placement.ClipY = top;
	placement.ClipWidth = right - left;
	placement.ClipHeight = bottom - top;

	return(placement.ClipWidth > 0 && placement.ClipHeight > 0);
}


/// <summary>
/// Starts the presenter on the game's window.
/// </summary>
/// <param name="window">The native window whose drawable area receives the frame.</param>
/// <param name="drawablewidth">The drawable area's width in physical pixels.</param>
/// <param name="drawableheight">The drawable area's height in physical pixels.</param>
/// <param name="refreshrate">The display refresh rate in hertz, or zero when unknown.</param>
/// <returns>bool; Did the presenter start? A false return is fatal to the game.</returns>
bool Video_Init(NativeWindow const & window, int drawablewidth, int drawableheight, int refreshrate)
{
	if (_Initialized) {
		return(true);
	}

	if (window.Handle == nullptr || drawablewidth <= 0 || drawableheight <= 0) {
		return(false);
	}

	_ScaleInfo.DrawableWidth = drawablewidth;
	_ScaleInfo.DrawableHeight = drawableheight;

	BackendRenderer renderer = (BackendRenderer)Options.Renderer;
	if (!Backend_Init(window, drawablewidth, drawableheight, renderer, Options.VSync)) {
		return(false);
	}

	DebugString("Video: renderer is %s\n", Backend_Renderer_Name());

	_Initialized = true;

	if (!Backend_Set_Frame_Size(VideoModeWidth, VideoModeHeight, Frame_Transparent_Pixel(VideoModeWidth, VideoModeHeight, VideoResolutionWidth, VideoResolutionHeight, VideoWorldHeight))) {
		Backend_Shutdown();
		_Initialized = false;
		return(false);
	}

	Update_Scale_Info();
	Update_Present_Interval(refreshrate);
	return(true);
}


/// <summary>
/// Stops the presenter and releases the renderer.
/// </summary>
void Video_Shutdown(void)
{
	if (!_Initialized) {
		return;
	}

	Win_Cursor_Shutdown();
	Backend_Shutdown();
	_Initialized = false;
	_FrameIsDirty = false;

	delete [] _WorldPixels;
	_WorldPixels = NULL;
	_WorldVisible = false;
}


/// <summary>
/// Moves the game to a different render resolution.
/// The caller replaces the surfaces afterwards; this only resizes what the frame is
/// presented from and leaves the previous mode untouched when it fails.
/// </summary>
/// <param name="width">The new frame width.</param>
/// <param name="height">The new frame height.</param>
/// <param name="resolutionwidth">The resolution's width, which differs from the frame's only
/// while the interface is scaled.</param>
/// <param name="resolutionheight">The resolution's height.</param>
/// <returns>bool; Was the mode changed?</returns>
bool Video_Set_Mode(int width, int height, int resolutionwidth, int resolutionheight)
{
	if (!_Initialized || width <= 0 || height <= 0 || resolutionwidth <= 0 || resolutionheight <= 0) {
		return(false);
	}

	if (!Backend_Set_Frame_Size(width, height, Frame_Transparent_Pixel(width, height, resolutionwidth, resolutionheight, VideoWorldHeight))) {
		return(false);
	}

	VideoModeWidth = width;
	VideoModeHeight = height;
	VideoResolutionWidth = resolutionwidth;
	VideoResolutionHeight = resolutionheight;

	// The surfaces the map's picture came from are about to be replaced.
	_WorldVisible = false;

	Update_Scale_Info();
	Win_Cursor_Refresh();
	_FrameIsDirty = true;
	return(true);
}


/// <summary>
/// Changes how many pixels of the tactical map the screen is tall.
/// The caller replaces the tactical map's surfaces afterwards; this only prepares the frame
/// and leaves the previous height in place when it fails.
/// </summary>
/// <param name="height">The new height, or zero to draw the map at the resolution.</param>
/// <returns>bool; Was the height changed?</returns>
bool Video_Set_World_Height(int height)
{
	if (!_Initialized) {
		return(false);
	}

	if (!Backend_Set_Frame_Size(VideoModeWidth, VideoModeHeight, Frame_Transparent_Pixel(VideoModeWidth, VideoModeHeight, VideoResolutionWidth, VideoResolutionHeight, height))) {
		return(false);
	}

	VideoWorldHeight = height;

	// The surfaces the map's picture came from are about to be replaced.
	_WorldVisible = false;

	Update_Scale_Info();
	_FrameIsDirty = true;
	return(true);
}


/// <summary>
/// Tells the presenter the drawable area changed size.
/// </summary>
void Video_On_Resize(int drawablewidth, int drawableheight)
{
	if (!_Initialized || drawablewidth <= 0 || drawableheight <= 0) {
		return;
	}

	_ScaleInfo.DrawableWidth = drawablewidth;
	_ScaleInfo.DrawableHeight = drawableheight;
	Backend_On_Resize(drawablewidth, drawableheight);
	Update_Scale_Info();
	Win_Cursor_Refresh();
	Video_Mark_Dirty();
}


/// <summary>
/// Sets the refresh rate used to pace presentation.
/// </summary>
void Video_Set_Refresh_Rate(int refreshrate)
{
	if (!_Initialized) {
		return;
	}

	Update_Present_Interval(refreshrate);
	Video_Mark_Dirty();
}


/// <summary>
/// Shows a picture of the tactical map beneath the frame. The map shows through wherever
/// the frame holds VIDEO_TRANSPARENT_PIXEL. The picture is copied, so the surface may be
/// drawn on again straight away.
/// </summary>
/// <param name="surface">The surface holding the picture.</param>
/// <param name="source">The part of the surface to show.</param>
/// <param name="dest">Where in the frame to show it.</param>
/// <param name="shiftx">How far to push the picture sideways within that place, in pixels
/// of the surface. The strip it uncovers stays black.</param>
/// <param name="shifty">How far to push the picture down within that place.</param>
void Video_Show_World(Surface & surface, Rect const & source, Rect const & dest, int shiftx, int shifty)
{
	if (!_Initialized) {
		return;
	}

	DSurface & dsurface = (DSurface &)surface;
	unsigned char const * pixels = (unsigned char const *)dsurface.Get_Buffer();
	int width = dsurface.Get_Width();
	int height = dsurface.Get_Height();

	if (pixels == NULL || width <= 0 || height <= 0) {
		return;
	}

	if (_WorldPixels == NULL || _WorldWidth != width || _WorldHeight != height) {
		delete [] _WorldPixels;
		_WorldPixels = new unsigned short[width * height];
		_WorldWidth = width;
		_WorldHeight = height;
	}

	for (int y = 0; y < height; y++) {
		memcpy(_WorldPixels + y * width, pixels + y * dsurface.Stride(), width * sizeof(unsigned short));
	}

	_WorldSource = source;
	_WorldDest = dest;
	_WorldShiftX = shiftx;
	_WorldShiftY = shifty;
	_WorldVisible = true;
	_WorldChanged = true;
	_FrameIsDirty = true;
}


/// <summary>
/// Stops showing the tactical map beneath the frame.
/// </summary>
void Video_Hide_World(void)
{
	if (_WorldVisible) {
		_WorldVisible = false;
		_FrameIsDirty = true;
	}
}


/// <summary>
/// Records that the visible surface has been drawn to since the last present.
/// </summary>
void Video_Mark_Dirty(void)
{
	_FrameIsDirty = true;
}


/// <summary>
/// Puts the visible surface on the screen whatever its state.
/// </summary>
void Video_Present(void)
{
	if (!_Initialized || _Presenting || VisibleSurface == NULL) {
		return;
	}

	DSurface * surface = (DSurface *)VisibleSurface;
	void * pixels = surface->Get_Buffer();

	if (pixels == NULL) {
		return;
	}

	_Presenting = true;

	BackendPlacement world;
	BackendPlacement const * world_placement = NULL;
	if (_WorldVisible && Place_World(world)) {
		world_placement = &world;
	}

	Backend_Present(pixels, surface->Stride(), _ScaleInfo.DestX, _ScaleInfo.DestY, _ScaleInfo.DestWidth, _ScaleInfo.DestHeight, Backend_Scale_Mode(), world_placement);
	_Presenting = false;

	_FrameIsDirty = false;
	_LastPresentTime = timeGetTime();
}


/// <summary>
/// Puts the visible surface on the screen if it has changed and the display is ready for
/// another frame.
/// A skipped present leaves the frame marked, so the next one shows the newest content.
/// This never waits: the game loop is not paced by presentation.
/// </summary>
void Video_Present_If_Dirty(void)
{
	if (!_FrameIsDirty) {
		return;
	}

	unsigned int now = timeGetTime();
	if ((now - _LastPresentTime) < _PresentInterval) {
		return;
	}

	Video_Present();
}


/// <summary>
/// Reports where the game's frame is drawn inside the window.
/// </summary>
VideoScaleInfo const & Video_Get_Scale_Info(void)
{
	return(_ScaleInfo);
}


/// <summary>
/// Compares two display modes by width and then height.
/// </summary>
static int __cdecl Compare_Modes(void const * left, void const * right)
{
	int const * lhs = (int const *)left;
	int const * rhs = (int const *)right;

	if (lhs[0] != rhs[0]) {
		return(lhs[0] - rhs[0]);
	}
	return(lhs[1] - rhs[1]);
}


/// <summary>
/// Collects the display resolutions that fall within the given bounds.
/// Only the sizes matter; the desktop decides the color depth, and duplicates that differ
/// only by refresh rate are reported once.
/// </summary>
/// <param name="minwidth">The narrowest mode to report.</param>
/// <param name="minheight">The shortest mode to report.</param>
/// <param name="maxwidth">The widest mode to report.</param>
/// <param name="maxheight">The tallest mode to report.</param>
/// <returns>A caller owned array of width and height pairs ending in a zero pair, or NULL
/// when nothing matched.</returns>
int * EnumDisplayModes(int minwidth, int minheight, int maxwidth, int maxheight)
{
	DEVMODE devmode;
	int count = 0;
	int capacity = 0;
	int * modes = NULL;

	for (int pass = 0; pass < 2; pass++) {

		count = 0;

		for (int index = 0; ; index++) {
			memset(&devmode, 0, sizeof(devmode));
			devmode.dmSize = sizeof(devmode);

			if (!EnumDisplaySettings(NULL, index, &devmode)) {
				break;
			}

			int width = (int)devmode.dmPelsWidth;
			int height = (int)devmode.dmPelsHeight;

			if (width < minwidth || width > maxwidth || height < minheight || height > maxheight) {
				continue;
			}

			if (modes != NULL) {
				// The list is being filled from a second enumeration; should it have
				// grown since the one that sized the array, the extra modes are dropped.
				if (count >= capacity) {
					break;
				}
				modes[count * 2] = width;
				modes[count * 2 + 1] = height;
			}
			count++;
		}

		if (modes != NULL) {
			break;
		}

		if (count == 0) {
			return(NULL);
		}

		capacity = count;
		modes = new int[(count + 1) * 2];
	}

	qsort(modes, count, sizeof(int) * 2, Compare_Modes);

	// The same size is listed once per refresh rate and color depth it supports.
	int unique = 0;
	for (int index = 0; index < count; index++) {
		if (unique == 0 || modes[unique * 2 - 2] != modes[index * 2] || modes[unique * 2 - 1] != modes[index * 2 + 1]) {
			modes[unique * 2] = modes[index * 2];
			modes[unique * 2 + 1] = modes[index * 2 + 1];
			unique++;
		}
	}

	modes[unique * 2] = 0;
	modes[unique * 2 + 1] = 0;
	return(modes);
}
