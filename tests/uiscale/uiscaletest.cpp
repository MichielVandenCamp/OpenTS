/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include <cstdio>

#include "uiscale.h"


static int _Failures = 0;


static void Check(bool condition, char const * description)
{
	std::printf("%-64s %s\n", description, condition ? "ok" : "FAILED");
	if (!condition) {
		_Failures++;
	}
}


static bool Frame_Is(int width, int height, int uiheight, int expectedwidth, int expectedheight)
{
	int framewidth = 0;
	int frameheight = 0;
	Interface_Frame_Size(width, height, uiheight, framewidth, frameheight);
	if (framewidth != expectedwidth || frameheight != expectedheight) {
		std::printf("  %dx%d at %d gave %dx%d, expected %dx%d\n", width, height, uiheight, framewidth, frameheight, expectedwidth, expectedheight);
		return(false);
	}
	return(true);
}


// A position handed from one span to the other and back must come home, whichever of the
// two is the finer; scrolling warps the cursor through exactly that trip.
static bool Round_Trip_Is_Stable(int from, int to)
{
	for (int pixel = -from; pixel < 2 * from; pixel++) {
		int there = Scale_Frame_Pixel(pixel, from, to);
		int back = Scale_Frame_Pixel(there, to, from);
		if (Scale_Frame_Pixel(back, from, to) != there) {
			std::printf("  %d on %d -> %d on %d -> %d does not return\n", pixel, from, there, to, back);
			return(false);
		}
	}
	return(true);
}


int main(void)
{
	Check(Frame_Is(1920, 1080, 0, 1920, 1080), "no interface height keeps the resolution");
	Check(Frame_Is(1920, 1080, -5, 1920, 1080), "a negative interface height keeps the resolution");
	Check(Frame_Is(1920, 1080, 1080, 1920, 1080), "the resolution's own height keeps the resolution");
	Check(Frame_Is(800, 600, 600, 800, 600), "800x600 at 600 lines is unchanged");
	Check(Frame_Is(1920, 1080, 600, 1067, 600), "1920x1080 at 600 lines keeps its shape");
	Check(Frame_Is(3840, 2160, 600, 1067, 600), "3840x2160 at 600 lines matches 1920x1080");
	Check(Frame_Is(800, 600, 1080, 1440, 1080), "a frame may be larger than the resolution");
	Check(Frame_Is(1280, 1024, 480, 640, 512), "a frame narrower than 640 is widened");
	Check(Frame_Is(1920, 1080, 300, 711, 400), "a frame shorter than 400 is heightened");
	Check(Frame_Is(1920, 1080, 5000, 4096, 2304), "a frame wider than 4096 is narrowed");

	Check(Scale_Frame_Edge(0, 1067, 1920) == 0, "the start of a span stays at the start");
	Check(Scale_Frame_Edge(1067, 1067, 1920) == 1920, "the end of a span lands on the end");
	Check(Scale_Frame_Edge(899, 1067, 1920) == 1618, "an edge rounds to the nearest boundary");
	Check(Scale_Frame_Edge(16, 600, 1080) == 29, "the tab strip's edge rounds to the nearest boundary");
	Check(Scale_Frame_Edge(5, 0, 100) == 5, "an empty span leaves an edge alone");

	Check(Scale_Frame_Pixel(0, 600, 1080) == 0, "the first pixel maps to the first pixel");
	Check(Scale_Frame_Pixel(599, 600, 1080) == 1079, "the last pixel maps to the last pixel");
	Check(Scale_Frame_Pixel(-1, 600, 1080) == -1, "a pixel before the span stays before it");
	Check(Scale_Frame_Pixel(7, 640, 640) == 7, "equal spans map a pixel to itself");

	Check(Round_Trip_Is_Stable(899, 1618), "a position survives a trip to a finer span and back");
	Check(Round_Trip_Is_Stable(584, 1051), "a position survives a trip at 1.8 times");
	Check(Round_Trip_Is_Stable(1440, 800), "a position survives a trip to a coarser span and back");
	Check(Round_Trip_Is_Stable(1000, 1001), "a position survives a trip at nearly one to one");
	Check(Round_Trip_Is_Stable(3, 7), "a position survives a trip between small spans");
	Check(Round_Trip_Is_Stable(7, 3), "a position survives a trip between small spans reversed");

	return(_Failures == 0 ? 0 : 1);
}
