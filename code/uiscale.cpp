/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "uiscale.h"

#include <algorithm>
#include <cmath>


// The display options offer resolutions within these bounds, and the interface is never
// laid out on a frame outside them either.
static constexpr int MIN_FRAME_WIDTH = 640;
static constexpr int MIN_FRAME_HEIGHT = 400;
static constexpr int MAX_FRAME_SIZE = 4096;


static long long Floor_Divide(long long numerator, long long denominator)
{
	long long quotient = numerator / denominator;
	if ((numerator % denominator) != 0 && numerator < 0) {
		quotient--;
	}
	return(quotient);
}


/// <summary>
/// Works out the frame the interface is laid out on.
/// The frame keeps the resolution's shape. A height that would make the frame smaller than
/// the smallest resolution the game supports, or larger than the largest, is moved to the
/// nearest height that fits.
/// </summary>
/// <param name="width">The resolution's width.</param>
/// <param name="height">The resolution's height.</param>
/// <param name="uiheight">How many pixels tall the frame should be, or zero or less for a
/// frame the size of the resolution.</param>
/// <param name="framewidth">Receives the frame's width.</param>
/// <param name="frameheight">Receives the frame's height.</param>
void Interface_Frame_Size(int width, int height, int uiheight, int & framewidth, int & frameheight)
{
	framewidth = width;
	frameheight = height;

	if (uiheight <= 0 || width <= 0 || height <= 0) {
		return;
	}

	int lowest = std::max(MIN_FRAME_HEIGHT, (int)Floor_Divide((long long)MIN_FRAME_WIDTH * height + width - 1, width));
	int highest = std::min(MAX_FRAME_SIZE, (int)Floor_Divide((long long)MAX_FRAME_SIZE * height, width));

	frameheight = std::clamp(uiheight, lowest, std::max(lowest, highest));
	framewidth = Scale_Frame_Edge(width, height, frameheight);
}


/// <summary>
/// Moves a boundary between pixels from a span of one size to the same place on a span of
/// another, rounded to the nearest boundary. Rectangles converted edge by edge still meet
/// wherever they met before.
/// </summary>
/// <param name="edge">The boundary's distance from the start of the span.</param>
/// <param name="from">The size of the span the boundary is measured on.</param>
/// <param name="to">The size of the span to move it onto.</param>
/// <returns>int; The boundary's distance from the start of the other span.</returns>
int Scale_Frame_Edge(int edge, int from, int to)
{
	if (from <= 0) {
		return(edge);
	}
	return((int)Floor_Divide(2LL * edge * to + from, 2LL * from));
}


/// <summary>
/// Finds the pixel of another span that the middle of a pixel lands on.
/// Going from the finer span to the coarser one and back returns the pixel started from,
/// so a position can be handed between the two without creeping.
/// </summary>
/// <param name="pixel">The pixel's distance from the start of the span, which may fall
/// outside it.</param>
/// <param name="from">The size of the span the pixel belongs to.</param>
/// <param name="to">The size of the span to find the matching pixel in.</param>
/// <returns>int; The matching pixel's distance from the start of the other span.</returns>
int Scale_Frame_Pixel(int pixel, int from, int to)
{
	if (from <= 0) {
		return(pixel);
	}
	return((int)Floor_Divide((2LL * pixel + 1) * to, 2LL * from));
}


/// <summary>
/// Works out how many pixels of the tactical map the screen is tall at a zoom step.
/// </summary>
/// <param name="step">The zoom step, from zero for the nearest to WORLD_ZOOM_STEPS for the
/// farthest. A step outside that range is moved to the nearer end of it.</param>
/// <returns>int; The screen's height in pixels of the map.</returns>
int World_Zoom_Height(int step)
{
	step = std::clamp(step, 0, WORLD_ZOOM_STEPS);

	double ratio = (double)WORLD_ZOOM_FARTHEST_HEIGHT / (double)WORLD_ZOOM_NEAREST_HEIGHT;
	return((int)std::lround(WORLD_ZOOM_NEAREST_HEIGHT * std::pow(ratio, (double)step / (double)WORLD_ZOOM_STEPS)));
}
