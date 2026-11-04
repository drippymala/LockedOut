//---------------------------------------------------------
// file:	utils.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:   Header file containing declarations for utility
//			functions used throughout the game, including click
//			detection, circle collision detection, colour and
//			direction helpers, and drawing helpers.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include "cprocessing.h"   // needed for CP_Color and CP_Vector

/* Click / collision */
int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y);
int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y);
int AreCirclesIntersecting(float c1_x, float c1_y, float r1, float c2_x, float c2_y, float r2);

/* Math / color */
CP_Color  Col(int r, int g, int b, int a);
CP_Vector GetNormalizedDirection(CP_Vector direction);

/* Drawing helpers */
void DrawFilledCircle(float centerX, float centerY, float radius, CP_Color color);
void DrawCircleOutline(float centerX, float centerY, float radius, float lineThickness, CP_Color color);
void DrawColoredLine(float startX, float startY, float endX, float endY, float lineThickness, CP_Color color);
void G_Circle(float x, float y, float radius, CP_Color c);
void G_Ring(float x, float y, float radius, float weight, CP_Color c);
void G_Rect(float x, float y, float w, float h, CP_Color c);

typedef enum { AL_LEFT, AL_CENTER, AL_RIGHT } Align;   // text alignment for G_Text

void G_Text(const char* s, float x, float y, float size, CP_Color c, Align a);