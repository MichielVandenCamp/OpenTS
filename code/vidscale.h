/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Conversions between the window's own pixels and the frame the game draws in. The two
// differ whenever the frame is scaled or letterboxed to fit the window, so anything that
// reads a position from Windows has to come through here before the game sees it. The
// frame and the tactical map's own surface differ in turn whenever the view's zoom does not
// match the interface's scale.

#pragma once

#include "rect.h"
#include "win.h"


bool Video_Scaling_Active(void);

void Window_Point_To_Game(POINT & point);
void Game_Point_To_Window(POINT & point);
void Screen_Point_To_Game(POINT & point);
void Game_Point_To_Screen(POINT & point);

void Clamp_To_Game(POINT & point);
void Get_Logical_Cursor_Pos(HWND window, POINT & point);

bool Tactical_Is_Scaled(void);
Rect Tactical_Surface_Rect(Rect const & view);
Point2D Screen_To_Tactical(Point2D const & point);
Point2D Tactical_To_Screen(Point2D const & point);
