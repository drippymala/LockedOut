//---------------------------------------------------------
// file:	splash.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Splash screen game state. Fades in the DigiPen logo
//			and the copyright text over a dark background with a
//			soft red glow, holds them, fades out, and then goes
//			to the main menu. The logo is fully visible for at
//			least two seconds.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include <stdbool.h>

#include "cprocessing.h"
#include "utils.h"        // Col, G_Rect, G_Text, DrawFilledCircle
#include "mainmenu.h"     // Font_EnsureLoaded, Main_Menu_* (the next state)
#include "splash.h"

#define LOGO_PATH            "Assets/DigiPen_BLACK.png"
// "\xC2\xA9" is the (c) in a circle written as UTF-8 bytes, so it shows correctly whatever encoding this file is saved in
#define COPYRIGHT_TEXT       "All content \xC2\xA9 2026 DigiPen Institute of Technology Singapore. All Rights Reserved."

#define SPLASH_FADE_IN       0.6f    // seconds to fade in
#define SPLASH_HOLD          2.0f    // seconds the logo stays fully visible (this is the "at least two seconds")
#define SPLASH_FADE_OUT      0.6f    // seconds to fade out
#define SPLASH_TOTAL         (SPLASH_FADE_IN + SPLASH_HOLD + SPLASH_FADE_OUT)
#define SPLASH_SKIP_AFTER    2.0f    // a key press or click can skip the splash, but only after this many seconds

#define LOGO_WIDTH           640.0f  // logo width on screen at 1600x900 (the height keeps the logo's shape)
#define GLOW_RINGS           14      // stacked circles that make the soft red glow behind the logo

static CP_Image s_logo = NULL;
static float    s_w = 1600.0f, s_h = 900.0f;   // window size
static float    s_k = 1.0f;                    // UI scale: 1.0 at 1600x900, smaller for smaller windows
static float    s_time;                        // seconds since the splash started
static float    s_logoW, s_logoH;              // logo size on screen

static void GoToMainMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

// 0..1: how visible everything is right now (fade in, hold, fade out).
static float GetVisibility(void)
{
	float v = 1.0f;
	if (s_time < SPLASH_FADE_IN)                          v = s_time / SPLASH_FADE_IN;
	else if (s_time > SPLASH_FADE_IN + SPLASH_HOLD)       v = (SPLASH_TOTAL - s_time) / SPLASH_FADE_OUT;
	return CP_Math_ClampFloat(v, 0.0f, 1.0f);
}

void Splash_Init(void)
{
	Font_EnsureLoaded();   // the copyright text needs the font

	s_w = (float)CP_System_GetWindowWidth();
	s_h = (float)CP_System_GetWindowHeight();
	s_k = (s_w / 1600.0f < s_h / 900.0f) ? s_w / 1600.0f : s_h / 900.0f;
	s_time = 0.0f;

	if (!s_logo) s_logo = CP_Image_Load(LOGO_PATH);
	if (s_logo)
	{
		s_logoW = LOGO_WIDTH * s_k;
		s_logoH = s_logoW * (float)CP_Image_GetHeight(s_logo) / (float)CP_Image_GetWidth(s_logo);
	}
	else
	{
		s_logoW = 0.0f;   // logo file not found: the splash still shows the text, then moves on
		s_logoH = 0.0f;
	}
}

void Splash_Update(void)
{
	float dt = CP_System_GetDt();
	if (dt > 0.05f) dt = 0.05f;
	s_time += dt;

	// ---- leave: when the time is up, or (after the minimum time) when the player skips ----
	bool skip = CP_Input_KeyTriggered(KEY_ESCAPE) || CP_Input_KeyTriggered(KEY_SPACE) ||
	            CP_Input_KeyTriggered(KEY_ENTER) || CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);
	if (s_time >= SPLASH_TOTAL || (skip && s_time >= SPLASH_SKIP_AFTER))
	{
		GoToMainMenu();
		return;
	}

	// ---- draw ----
	float visible = GetVisibility();
	int   alpha = (int)(255.0f * visible);
	float cx = s_w * 0.5f;
	float cy = s_h * 0.44f;   // logo centre: a little above the middle of the screen

	// background: near-black with a soft red glow behind the logo (big faint circles stacked up)
	CP_Graphics_ClearBackground(Col(10, 12, 10, 255));
	for (int i = 0; i < GLOW_RINGS; ++i)
	{
		float radius = (600.0f - 40.0f * (float)i) * s_k;
		DrawFilledCircle(cx, cy, radius, Col(150, 20, 25, (int)(7.0f * visible)));
	}

	// logo
	if (s_logo)
	{
		CP_Settings_ImageMode(CP_POSITION_CENTER);
		CP_Image_Draw(s_logo, cx, cy, s_logoW, s_logoH, alpha);
	}
	else
	{
		G_Text("DigiPen Institute of Technology Singapore", cx, cy, 48.0f * s_k, Col(235, 235, 235, alpha), AL_CENTER);
	}

	// thin red line and the copyright text at the bottom
	G_Rect(cx - 450.0f * s_k, s_h - 96.0f * s_k, 900.0f * s_k, 2.0f * s_k, Col(140, 30, 30, alpha));
	G_Text(COPYRIGHT_TEXT, cx, s_h - 62.0f * s_k, 22.0f * s_k, Col(210, 210, 210, alpha), AL_CENTER);

	// once skipping is allowed, tell the player they can press SPACE
	if (s_time >= SPLASH_SKIP_AFTER)
	{
		G_Text("Press SPACE to skip", cx, s_h - 26.0f * s_k, 18.0f * s_k, Col(150, 150, 150, (int)(alpha * 0.8f)), AL_CENTER);
	}
}

void Splash_Exit(void)
{
	if (s_logo)
	{
		CP_Image_Free(&s_logo);   // CProcessing frees an image through a pointer to it
		s_logo = NULL;
	}
	// the font stays loaded (see Main_Menu_Exit), so the main menu doesn't have to load it again
}
