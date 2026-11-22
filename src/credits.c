//---------------------------------------------------------
// file:	credits.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Credits game state. The DigiPen logo and the copyright
//			text stay pinned at the top of the screen while the
//			names roll up underneath them (and loop).
//			Opened from the main menu, and goes back to it.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include <stdbool.h>

#include "cprocessing.h"
#include "utils.h"        // Col, G_Rect, G_Text
#include "mainmenu.h"     // Font_EnsureLoaded, myFont, Main_Menu_* (to go back)
#include "credits.h"

#define LOGO_PATH            "Assets/DigiPen_BLACK.png"
// "\xC2\xA9" is the (c) in a circle written as UTF-8 bytes, so it shows correctly whatever encoding this file is saved in
#define COPYRIGHT_TEXT       "All content \xC2\xA9 2026 DigiPen Institute of Technology Singapore. All Rights Reserved."

#define SCROLL_SPEED         75.0f    // px per second the names roll up (at 1600x900)
#define SCROLL_FAST_FACTOR   4.0f     // hold SPACE / DOWN to roll this much faster
#define HEADER_HEIGHT        170.0f   // the pinned logo + copyright area at the top
#define FOOTER_HEIGHT        36.0f    // the pinned hint bar at the bottom

/* =========================================================
   The rolling text
   Add / edit / reorder lines here. Names are shown exactly as written.
   ========================================================= */
typedef enum { CL_TITLE, CL_HEADING, CL_NAME, CL_GAP, CL_END } CreditType;
typedef struct { CreditType type; const char* text; } CreditLine;

static const CreditLine s_lines[] =
{
	{ CL_TITLE,   "BLEED OUT" },
	{ CL_GAP,     "" },

	{ CL_HEADING, "TEAM MEMBERS" },
	{ CL_NAME,    "Syahmi" },
	{ CL_NAME,    "Sherilyn" },
	{ CL_NAME,    "Sean" },
	{ CL_NAME,    "Joshua" },
	{ CL_GAP,     "" },

	{ CL_HEADING, "INSTRUCTORS" },
	{ CL_NAME,    "Gerald Wong" },
	{ CL_NAME,    "Dr. Soroor" },
	{ CL_NAME,    "Tommy Tan" },
	{ CL_GAP,     "" },

	{ CL_HEADING, "MUSIC" },
	{ CL_NAME,    "Title music:  \"8 Bit Dungeon Boss\"  by Kevin MacLeod (incompetech.com)" },
	{ CL_NAME,    "Licensed under Creative Commons: By Attribution 4.0" },
	{ CL_NAME,    "creativecommons.org/licenses/by/4.0/" },
	{ CL_NAME,    "Gameplay music:  \"Run As Fast As You Can\"  by Pix" },
	{ CL_NAME,    "Final boss music:  \"Boss Time\"  by FesilyanStudios" },
	{ CL_GAP,     "" },

	{ CL_HEADING, "PRESIDENT" },
	{ CL_NAME,    "Claude Comair" },
	{ CL_GAP,     "" },

	{ CL_HEADING, "EXECUTIVES" },
	{ CL_NAME,    "CHU Jason Yeu Tat" },
	{ CL_NAME,    "Michael GATS" },
	{ CL_NAME,    "Tan Chek Min" },
	{ CL_NAME,    "Prasanna Kumar GHALI" },
	{ CL_NAME,    "Mandy WONG" },
	{ CL_NAME,    "Johnny DEEK" },
	{ CL_GAP,     "" },
	{ CL_GAP,     "" },

	{ CL_END,     "Thanks for playing!" }
};
#define LINE_COUNT  ((int)(sizeof(s_lines) / sizeof(s_lines[0])))

// How tall each kind of line is (at 1600x900) and how big its text is.
static float LineHeight(CreditType t)
{
	switch (t)
	{
	case CL_TITLE:   return 110.0f;
	case CL_HEADING: return 56.0f;
	case CL_NAME:    return 42.0f;
	case CL_END:     return 80.0f;
	default:         return 60.0f;   // CL_GAP
	}
}

/* =========================================================
   State data
   ========================================================= */
typedef struct { float x, y, w, h; const char* label; } CreditsButton;

static CreditsButton s_backButton;
static CP_Image      s_logo = NULL;
static float         s_logoW, s_logoH;       // logo size on screen (keeps its shape)
static float         s_w = 1600.0f, s_h = 900.0f;
static float         s_k = 1.0f;             // UI scale: 1.0 at 1600x900, smaller for smaller windows
static float         s_scrollY;              // y of the top of the first line
static float         s_totalH;               // height of all the lines together

static bool Button_Hovered(const CreditsButton* b, float mx, float my)
{
	return mx >= b->x && mx <= b->x + b->w && my >= b->y && my <= b->y + b->h;
}

// Same look as the main menu buttons.
static void Button_Draw(const CreditsButton* b, float mx, float my)
{
	bool hover = Button_Hovered(b, mx, my);

	G_Rect(b->x - 4.0f * s_k, b->y - 4.0f * s_k, b->w + 8.0f * s_k, b->h + 8.0f * s_k,
		hover ? Col(255, 230, 120, 255) : Col(90, 20, 20, 255));
	G_Rect(b->x, b->y, b->w, b->h,
		hover ? Col(170, 30, 30, 255) : Col(60, 12, 12, 255));
	G_Text(b->label, b->x + b->w * 0.5f, b->y + b->h * 0.5f, 30.0f * s_k, Col(255, 255, 255, 255), AL_CENTER);
}

// A strip that fades from one see-through level to another (top to bottom), in the background colour.
// Used so the names fade out under the header and above the footer instead of being cut off.
static void DrawVerticalFade(float y, float h, int alphaTop, int alphaBottom)
{
	const int steps = 10;
	for (int i = 0; i < steps; ++i)
	{
		float t = ((float)i + 0.5f) / (float)steps;
		int alpha = (int)((float)alphaTop + (float)(alphaBottom - alphaTop) * t);
		G_Rect(0.0f, y + h * (float)i / (float)steps, s_w, h / (float)steps + 1.0f, Col(18, 22, 16, alpha));
	}
}

static void DrawLine(const CreditLine* line, float y, float lineHeight)
{
	float cx = s_w * 0.5f;
	float cy = y + lineHeight * 0.5f;

	switch (line->type)
	{
	case CL_TITLE:   G_Text(line->text, cx, cy, 80.0f * s_k, Col(200, 30, 30, 255), AL_CENTER); break;
	case CL_HEADING: G_Text(line->text, cx, cy, 36.0f * s_k, Col(255, 230, 120, 255), AL_CENTER); break;
	case CL_NAME:    G_Text(line->text, cx, cy, 30.0f * s_k, Col(235, 235, 235, 255), AL_CENTER); break;
	case CL_END:     G_Text(line->text, cx, cy, 44.0f * s_k, Col(200, 30, 30, 255), AL_CENTER); break;
	default: break;   // CL_GAP draws nothing
	}
}

static void GoBackToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   Game state callbacks
   ========================================================= */
void Credits_Init(void)
{
	Font_EnsureLoaded();   // the menu frees its font on exit, so load it again for this screen

	s_w = (float)CP_System_GetWindowWidth();
	s_h = (float)CP_System_GetWindowHeight();
	s_k = (s_w / 1600.0f < s_h / 900.0f) ? s_w / 1600.0f : s_h / 900.0f;

	// logo (white on a see-through background, so it shows on the dark screen). Loaded once and kept.
	if (!s_logo) s_logo = CP_Image_Load(LOGO_PATH);
	if (s_logo)
	{
		s_logoW = 300.0f * s_k;
		s_logoH = s_logoW * (float)CP_Image_GetHeight(s_logo) / (float)CP_Image_GetWidth(s_logo);
	}
	else
	{
		s_logoW = 0.0f;   // logo file not found: the screen still works, it just has no logo
		s_logoH = 0.0f;
	}

	// total height of the rolling text, and start it just below the bottom of the screen
	s_totalH = 0.0f;
	for (int i = 0; i < LINE_COUNT; ++i)
	{
		s_totalH += LineHeight(s_lines[i].type) * s_k;
	}
	s_scrollY = s_h;

	s_backButton = (CreditsButton){ 34.0f * s_k, 24.0f * s_k, 170.0f * s_k, 50.0f * s_k, "BACK" };
}

void Credits_Update(void)
{
	float dt = CP_System_GetDt();
	if (dt > 0.05f) dt = 0.05f;

	float mx = CP_Input_GetMouseX();
	float my = CP_Input_GetMouseY();
	bool click = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);

	// ---- input ----
	if (CP_Input_KeyTriggered(KEY_ESCAPE) || (click && Button_Hovered(&s_backButton, mx, my)))
	{
		GoBackToMenu();
		return;
	}

	// ---- roll the text up (hold SPACE or DOWN to roll faster) ----
	float speed = SCROLL_SPEED * s_k;
	if (CP_Input_KeyDown(KEY_SPACE) || CP_Input_KeyDown(KEY_DOWN)) speed *= SCROLL_FAST_FACTOR;
	s_scrollY -= speed * dt;

	// once the last line has rolled up under the header, start again from the bottom
	if (s_scrollY + s_totalH < HEADER_HEIGHT * s_k)
	{
		s_scrollY = s_h;
	}

	// ---- draw ----
	CP_Graphics_ClearBackground(Col(18, 22, 16, 255));

	// 1. the rolling text (only the lines that are on screen)
	float y = s_scrollY;
	for (int i = 0; i < LINE_COUNT; ++i)
	{
		float lineHeight = LineHeight(s_lines[i].type) * s_k;
		if (y + lineHeight > 0.0f && y < s_h)
		{
			DrawLine(&s_lines[i], y, lineHeight);
		}
		y += lineHeight;
	}

	// 2. pinned header on top of it: solid area with the logo + copyright, then a fade-out below it
	G_Rect(0.0f, 0.0f, s_w, HEADER_HEIGHT * s_k, Col(18, 22, 16, 255));
	DrawVerticalFade(HEADER_HEIGHT * s_k, 40.0f * s_k, 255, 0);

	if (s_logo)
	{
		CP_Settings_ImageMode(CP_POSITION_CENTER);
		CP_Image_Draw(s_logo, s_w * 0.5f, 22.0f * s_k + s_logoH * 0.5f, s_logoW, s_logoH, 255);
	}
	G_Text(COPYRIGHT_TEXT, s_w * 0.5f, 134.0f * s_k, 20.0f * s_k, Col(200, 200, 200, 255), AL_CENTER);
	G_Rect(s_w * 0.5f - 450.0f * s_k, 160.0f * s_k, 900.0f * s_k, 2.0f * s_k, Col(120, 30, 30, 255));   // divider line

	// 3. pinned footer with the hint, with a fade above it
	DrawVerticalFade(s_h - (FOOTER_HEIGHT + 40.0f) * s_k, 40.0f * s_k, 0, 255);
	G_Rect(0.0f, s_h - FOOTER_HEIGHT * s_k, s_w, FOOTER_HEIGHT * s_k, Col(18, 22, 16, 255));
	G_Text("Hold SPACE to speed up      ESC - back to menu",
		s_w * 0.5f, s_h - FOOTER_HEIGHT * 0.5f * s_k, 20.0f * s_k, Col(150, 150, 150, 255), AL_CENTER);

	Button_Draw(&s_backButton, mx, my);
}

void Credits_Exit(void)
{
	// the logo and the font stay loaded (see Main_Menu_Exit), so going back to the menu is instant
}
