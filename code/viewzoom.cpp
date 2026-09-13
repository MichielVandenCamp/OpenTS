/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "viewzoom.h"

#include "_map.h"
#include "_rect.h"
#include "_tactica.h"
#include "_xmouse.h"
#include "globals.h"
#include "gscreen.h"
#include "init.h"
#include "tactical.h"
#include "uiscale.h"
#include "video.h"
#include "vidscale.h"
#include "win.h"

#include <algorithm>


// The zoom step the view is drawn at, and the one the wheel has asked for since.
static int _ZoomStep = 0;
static int _PendingZoomStep = 0;

// Wheel movement short of a whole notch, which a fine-grained wheel or a touchpad reports.
static int _WheelRemainder = 0;


/// <summary>
/// Fetches how many pixels of the tactical map the screen is tall at the zoom in use.
/// </summary>
int View_Zoom_World_Height(void)
{
	return(World_Zoom_Height(_ZoomStep));
}


/// <summary>
/// Is the pointer over the tactical map while the player has the use of it?
/// </summary>
static bool Pointer_Is_Over_Map(void)
{
	if (!ScenarioActive || TacticalMap == NULL || MouseCursor == NULL) {
		return(false);
	}

	if (SpecialDialog != SDLG_NONE || _dialog_count > 0) {
		return(false);
	}

	return(TacticalScreenRect.Is_Point_Within(Get_Mouse_Point()));
}


/// <summary>
/// Measures a position on the screen from the middle of the tactical view, in pixels of the
/// map.
/// </summary>
static Point2D Offset_From_View_Center(Point2D const & point)
{
	return(Screen_To_Tactical(point) - Point2D(TacticalRect.Width / 2, TacticalRect.Height / 2));
}


/// <summary>
/// Offers a mouse wheel movement to the tactical view's zoom. Turning the wheel away from the
/// player zooms in. The movement takes effect when the next frame is drawn.
/// </summary>
/// <param name="delta">The movement as Windows reports it, positive away from the player.</param>
/// <returns>bool; Did the zoom take the movement? It takes none unless the pointer is over
/// the map and no dialog is up.</returns>
bool View_Zoom_Wheel(int delta)
{
	if (!Pointer_Is_Over_Map()) {
		_WheelRemainder = 0;
		return(false);
	}

	if ((delta < 0) != (_WheelRemainder < 0)) {
		_WheelRemainder = 0;
	}

	_WheelRemainder += delta;
	int notches = _WheelRemainder / WHEEL_DELTA;
	_WheelRemainder -= notches * WHEEL_DELTA;

	_PendingZoomStep = std::clamp(_PendingZoomStep - notches, 0, WORLD_ZOOM_STEPS);
	return(true);
}


/// <summary>
/// Applies the zoom step the wheel asked for. The spot on the map under the pointer stays
/// under it, or the middle of the view stays put while the pointer is off the map, as far as
/// the edges of the map allow. It replaces the tactical map's surfaces and has the whole
/// screen redrawn, so it may only be called between frames.
/// </summary>
void View_Zoom_Update(void)
{
	if (_PendingZoomStep == _ZoomStep || TacticalMap == NULL) {
		return;
	}

	Point2D anchor(TacticalScreenRect.X + TacticalScreenRect.Width / 2, TacticalScreenRect.Y + TacticalScreenRect.Height / 2);
	if (MouseCursor != NULL && TacticalScreenRect.Is_Point_Within(Get_Mouse_Point())) {
		anchor = Get_Mouse_Point();
	}

	Point2D target = TacticalMap->Get_Tactical_Position() + Offset_From_View_Center(anchor);

	if (!Video_Set_World_Height(World_Zoom_Height(_PendingZoomStep))) {
		_PendingZoomStep = _ZoomStep;
		return;
	}
	_ZoomStep = _PendingZoomStep;

	Allocate_Tactical_Surfaces(TacticalScreenRect);
	Map.Rescale_Tactical_View();
	TacticalMap->Set_Tactical_Position(target - Offset_From_View_Center(anchor));
	Map.Flag_To_Redraw(GS_REDRAW_ALL);
}
