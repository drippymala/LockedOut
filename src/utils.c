//---------------------------------------------------------
// file:	utils.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Source file containing the implementation of 
//			utility functions used by the game.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

//---------------------------------------------------------
// file:    utils.c
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