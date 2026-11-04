//---------------------------------------------------------
// file:	utils.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Source file containing the implementation of 
//			utility functions used by the game.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "cprocessing.h"
#include "utils.h"

int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y)
{
    float half_w = area_width / 2.0f;
    float half_h = area_height / 2.0f;

    float left = area_center_x - half_w;
    float right = area_center_x + half_w;
    float top = area_center_y - half_h;
    float bottom = area_center_y + half_h;

    if (click_x >= left && click_x <= right && click_y >= top && click_y <= bottom)
    {
        return 1;
    }
    return 0;
}

int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y)
{
    float radius = diameter / 2.0f;
    float dx = click_x - circle_center_x;
    float dy = click_y - circle_center_y;

    if ((dx * dx + dy * dy) <= (radius * radius))
    {
        return 1;
    }
    return 0;
}

int AreCirclesIntersecting(float c1_x, float c1_y, float r1, float c2_x, float c2_y, float r2)
{
    float dx = c2_x - c1_x;
    float dy = c2_y - c1_y;
    float radius_sum = r1 + r2;

    if ((dx * dx + dy * dy) <= (radius_sum * radius_sum))
    {
        return 1;
    }
    return 0;
}

CP_Color Col(int r, int g, int b, int a)
{
	return CP_Color_Create(r, g, b, a);
}

CP_Vector GetNormalizedDirection(CP_Vector direction)
{
	float length = CP_Vector_Length(direction);
	if (length < 0.0001f)
	{
		return CP_Vector_Set(0.0f, 0.0f);
	}
	return CP_Vector_Normalize(direction);
}

void DrawFilledCircle(float centerX, float centerY, float radius, CP_Color color)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(color);
	CP_Graphics_DrawCircle(centerX, centerY, radius * 2.0f);
}

void DrawCircleOutline(float centerX, float centerY, float radius, float lineThickness, CP_Color color)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoFill();
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawCircle(centerX, centerY, radius * 2.0f);
	CP_Settings_NoStroke();
}

void DrawColoredLine(float startX, float startY, float endX, float endY, float lineThickness, CP_Color color)
{
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawLine(startX, startY, endX, endY);
	CP_Settings_NoStroke();
}

void G_Circle(float x, float y, float radius, CP_Color c)
{
	DrawFilledCircle(x, y, radius, c);
}

void G_Ring(float x, float y, float radius, float weight, CP_Color c)
{
	DrawCircleOutline(x, y, radius, weight, c);
}

void G_Rect(float x, float y, float w, float h, CP_Color c)
{
	CP_Settings_RectMode(CP_POSITION_CORNER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawRect(x, y, w, h);
}

void G_Text(const char* s, float x, float y, float size, CP_Color c, Align a)
{
	CP_Settings_TextSize(size);
	CP_Settings_Fill(c);
	switch (a)
	{
	case AL_LEFT:   CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE); break;
	case AL_CENTER: CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE); break;
	default:        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_RIGHT, CP_TEXT_ALIGN_V_MIDDLE); break;
	}
	CP_Font_DrawText(s, x, y);
}