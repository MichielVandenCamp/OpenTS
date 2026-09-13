/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Layout arithmetic for an interface scaled apart from the tactical map. The map is drawn
// at one pixel per pixel of the resolution, while the interface is laid out on a smaller
// or larger frame that the presenter stretches over the same area.

#pragma once


void Interface_Frame_Size(int width, int height, int uiheight, int & framewidth, int & frameheight);

int Scale_Frame_Edge(int edge, int from, int to);
int Scale_Frame_Pixel(int pixel, int from, int to);
