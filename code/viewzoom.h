/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The player's zoom of the tactical view. The mouse wheel picks a zoom step while the
// pointer is over the map, and the step takes effect between frames because it replaces the
// map's drawing surfaces. The step lasts for the session and is not saved.

#pragma once


int View_Zoom_World_Height(void);
bool View_Zoom_Wheel(int delta);
void View_Zoom_Update(void);
