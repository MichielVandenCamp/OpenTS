/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Layout arithmetic for an interface scaled apart from the tactical map. The interface is
// laid out on a frame that the presenter stretches over the screen, and the map is drawn on
// a frame of its own whose size follows the view's zoom.

#pragma once


// The tactical view's zoom is the height of the screen measured in pixels of the map. Step
// zero, the nearest, shows as tall a slice of the map as an 800 by 600 screen, and the last
// step as tall a slice as a 1920 by 1080 one. Every step widens the view by the same ratio.
constexpr int WORLD_ZOOM_NEAREST_HEIGHT = 600;
constexpr int WORLD_ZOOM_FARTHEST_HEIGHT = 1080;
constexpr int WORLD_ZOOM_STEPS = 8;


void Interface_Frame_Size(int width, int height, int uiheight, int & framewidth, int & frameheight);

int Scale_Frame_Edge(int edge, int from, int to);
int Scale_Frame_Pixel(int pixel, int from, int to);

int World_Zoom_Height(int step);
