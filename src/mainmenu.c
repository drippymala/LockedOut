//---------------------------------------------------------
// file:	mainmenu.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
//
// brief:   Main menu state for the game, where the player 
//          can choose to start the game or exit
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "cprocessing.h"
#include "mainmenu.h"
#include "utils.h"
#include "game.h"

// Template globals (declared extern in mainmenu.h)
CP_Font myFont;
CP_Image myImage;


// CProcessing has no built-in font, so we load one ourselves.
// It tries your project's Assets folder first, then common Windows fonts.
void Font_EnsureLoaded(void)
{
	static const char* candidates[] =
	{
		"./Assets/Exo2-Regular.ttf",
		"Assets/Exo2-Regular.ttf",
		"C:/Windows/Fonts/arial.ttf",
		"C:/Windows/Fonts/segoeui.ttf",
		"C:/Windows/Fonts/calibri.ttf"
	};

	if (!myFont)
	{
		for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])) && !myFont; ++i)
		{
			FILE* f = fopen(candidates[i], "rb");        // only load files that really exist
			if (f)
			{
				fclose(f);
				myFont = CP_Font_Load(candidates[i]);
			}
		}
	}
	if (myFont) CP_Font_Set(myFont);
}

#define MENU_GHOSTS 16

typedef struct { float x, y, w, h; const char* label; } MenuButton;
typedef struct { CP_Vector pos, vel; float radius; } MenuGhost;

static MenuButton s_playButton, s_quitButton;
static MenuGhost  s_ghosts[MENU_GHOSTS];
static float      s_menuW = 1600.0f, s_menuH = 900.0f;
static float      s_menuTime;
static float      s_k = 1.0f;     // UI scale: 1.0 at 1600x900, smaller for smaller windows

typedef struct { int r, g, b; const char* text; } LegendLine;

static const LegendLine s_bulletLegend[] =
{
	{ 255, 230, 120, "1  PISTOL   - flat cost per shot" },
	{ 255, 150,  60, "2  TARGET LOCK	- health bar charges a homing shot" },
	{ 110, 255, 140, "3  PARASITE    - pay up front, refunded on hit" },
	{ 120, 200, 255, "4  MINIGUN	- cost increases while held" },
	{ 220, 120, 255, "5  HEALTH LASER	- health bar extends into a laser beam" }
};

static const LegendLine s_pickupLegend[] =
{
	{ 220,  40,  60, "Blood drop	- small heal (flies to you)" },
	{ 240, 240, 240, "Medkit     - instant heal" },
	{ 235, 200, 150, "Bandage    - heal over time" },
	{ 200,  80, 240, "Bloodlust	- kills heal you for 12s" },
	{ 255, 255, 255, "Landmine	 - place explosive triggers" }
};

/* ---- small draw helpers ---- */
static void M_Circle(float x, float y, float radius, CP_Color c)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawCircle(x, y, radius * 2.0f);      // diameter
}

static void M_Rect(float x, float y, float w, float h, CP_Color c)
{
	CP_Settings_RectMode(CP_POSITION_CORNER);        // CProcessing defaults to CENTER, so be explicit
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawRect(x, y, w, h);
}

static void M_Text(const char* s, float x, float y, float size, CP_Color c, bool centered)
{
	CP_Settings_TextSize(size);
	CP_Settings_Fill(c);
	CP_Settings_TextAlignment(centered ? CP_TEXT_ALIGN_H_CENTER : CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
	CP_Font_DrawText(s, x, y);
}

static bool M_ButtonHovered(const MenuButton* b, float mx, float my)
{
	// Check if the mouse is outside the button horizontally
	if (mx < (*b).x || mx >(*b).x + (*b).w)
	{
		return false;
	}

	// Check if the mouse is outside the button vertically
	if (my < (*b).y || my >(*b).y + (*b).h)
	{
		return false;
	}

	return true;
}

static void M_DrawButton(const MenuButton* b, float mx, float my)
{
	// Check if the mouse is hovering over the button
	bool hover = M_ButtonHovered(b, mx, my);

	// Draw the outer rectangle
	if (hover)
	{
		M_Rect((*b).x - 4.0f * s_k, (*b).y - 4.0f * s_k,
			(*b).w + 8.0f * s_k, (*b).h + 8.0f * s_k,
			CP_Color_Create(255, 230, 120, 255));
	}
	else
	{
		M_Rect((*b).x - 4.0f * s_k, (*b).y - 4.0f * s_k,
			(*b).w + 8.0f * s_k, (*b).h + 8.0f * s_k,
			CP_Color_Create(90, 20, 20, 255));
	}

	// Draw the inner rectangle
	if (hover)
	{
		M_Rect((*b).x, (*b).y, (*b).w, (*b).h,
			CP_Color_Create(170, 30, 30, 255));
	}
	else
	{
		M_Rect((*b).x, (*b).y, (*b).w, (*b).h,
			CP_Color_Create(60, 12, 12, 255));
	}

	// Draw the button text in the centre
	M_Text((*b).label,
		(*b).x + (*b).w * 0.5f,
		(*b).y + (*b).h * 0.5f,
		42.0f * s_k,
		CP_Color_Create(255, 255, 255, 255),
		true);
}

void Main_Menu_Init(void)
{
	Font_EnsureLoaded();

	s_menuW = (float)CP_System_GetWindowWidth();
	s_menuH = (float)CP_System_GetWindowHeight();
	s_menuTime = 0.0f;
	s_k = (s_menuW / 1600.0f < s_menuH / 900.0f) ? s_menuW / 1600.0f : s_menuH / 900.0f;

	s_playButton.w = 380.0f * s_k; s_playButton.h = 70.0f * s_k;
	s_playButton.x = (s_menuW - s_playButton.w) * 0.5f; s_playButton.y = 320.0f * s_k;
	s_playButton.label = "PLAY";

	s_quitButton = s_playButton;
	s_quitButton.y = 410.0f * s_k;
	s_quitButton.label = "QUIT";

	// background decoration: zombies shambling across the menu
	for (int i = 0; i < MENU_GHOSTS; ++i)
	{
		s_ghosts[i].pos = CP_Vector_Set(CP_Random_RangeFloat(0.0f, s_menuW), CP_Random_RangeFloat(0.0f, s_menuH));
		s_ghosts[i].vel = CP_Vector_Set(CP_Random_RangeFloat(-40.0f, 40.0f), CP_Random_RangeFloat(-40.0f, 40.0f));
		s_ghosts[i].radius = CP_Random_RangeFloat(14.0f, 26.0f);
	}
}

void Main_Menu_Update(void)
{
	float dt = CP_System_GetDt();
	if (dt > 0.05f) dt = 0.05f;
	s_menuTime += dt;

	float mx = CP_Input_GetMouseX();
	float my = CP_Input_GetMouseY();
	bool click = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);

	if ((click && M_ButtonHovered(&s_playButton, mx, my)) || CP_Input_KeyTriggered(KEY_ENTER) || CP_Input_KeyTriggered(KEY_SPACE))
	{
		CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
		return;
	}
	if (click && M_ButtonHovered(&s_quitButton, mx, my))
	{
		CP_Engine_Terminate();
		return;
	}

	// ---- draw ----
	CP_Graphics_ClearBackground(CP_Color_Create(18, 22, 16, 255));

	for (int i = 0; i < MENU_GHOSTS; ++i)
	{
		// Get the address of the current ghost
		MenuGhost* g = &s_ghosts[i];

		// Move the ghost according to its velocity and elapsed time
		(*g).pos.x += (*g).vel.x * dt;
		(*g).pos.y += (*g).vel.y * dt;

		// If the ghost moves past the left edge, move it to the right edge
		if ((*g).pos.x < -40.0f)
		{
			(*g).pos.x = s_menuW + 40.0f;
		}

		// If the ghost moves past the right edge, move it to the left edge
		if ((*g).pos.x > s_menuW + 40.0f)
		{
			(*g).pos.x = -40.0f;
		}

		// If the ghost moves past the top edge, move it to the bottom
		if ((*g).pos.y < -40.0f)
		{
			(*g).pos.y = s_menuH + 40.0f;
		}

		// If the ghost moves past the bottom edge, move it to the top
		if ((*g).pos.y > s_menuH + 40.0f)
		{
			(*g).pos.y = -40.0f;
		}

		// Draw the ghost's body
		M_Circle((*g).pos.x, (*g).pos.y, (*g).radius,
			CP_Color_Create(70, 120, 65, 90));

		// Draw the ghost's left eye
		M_Circle((*g).pos.x - (*g).radius * 0.35f,
			(*g).pos.y - (*g).radius * 0.2f,
			(*g).radius * 0.18f,
			CP_Color_Create(255, 40, 40, 120));

		// Draw the ghost's right eye
		M_Circle((*g).pos.x + (*g).radius * 0.35f,
			(*g).pos.y - (*g).radius * 0.2f,
			(*g).radius * 0.18f,
			CP_Color_Create(255, 40, 40, 120));
	}

	// title (gently pulsing)
	float pulse = 0.5f + 0.5f * sinf(s_menuTime * 2.5f);
	M_Text("BLEED OUT", s_menuW * 0.5f + 4.0f * s_k, 134.0f * s_k, 130.0f * s_k, CP_Color_Create(0, 0, 0, 200), true);
	M_Text("BLEED OUT", s_menuW * 0.5f, 130.0f * s_k, 130.0f * s_k, CP_Color_Create(200 + (int)(55.0f * pulse), 30, 30, 255), true);
	M_Text("Every bullet costs your blood.", s_menuW * 0.5f, 225.0f * s_k, 34.0f * s_k, CP_Color_Create(230, 230, 230, 255), true);

	M_DrawButton(&s_playButton, mx, my);
	M_DrawButton(&s_quitButton, mx, my);

	/* if (Game_GetBestWave() > 0)
	{
		char best[96];
		snprintf(best, sizeof(best), "Best this session: wave %d  /  score %d", Game_GetBestWave(), Game_GetBestScore());
		M_Text(best, s_menuW * 0.5f, 520.0f * s_k, 28.0f * s_k, CP_Color_Create(255, 230, 120, 255), true);
	} */

	// how to play
	float lx = s_menuW * 0.5f - 640.0f * s_k;
	float rx = s_menuW * 0.5f + 80.0f * s_k;
	float ty = 590.0f * s_k;
	M_Text("GUNS (randomized every 3 waves)", lx, ty, 28.0f * s_k, CP_Color_Create(255, 255, 255, 255), false);
	M_Text("PICKUPS (from enemies drops)", rx, ty, 28.0f * s_k, CP_Color_Create(255, 255, 255, 255), false);
	for (int i = 0; i < 5; ++i)
	{
		float y = ty + (38.0f + 34.0f * (float)i) * s_k;
		M_Circle(lx + 8.0f * s_k, y, 8.0f * s_k, CP_Color_Create(s_bulletLegend[i].r, s_bulletLegend[i].g, s_bulletLegend[i].b, 255));
		M_Text(s_bulletLegend[i].text, lx + 28.0f * s_k, y, 25.0f * s_k, CP_Color_Create(220, 220, 220, 255), false);
		M_Circle(rx + 8.0f * s_k, y, 8.0f * s_k, CP_Color_Create(s_pickupLegend[i].r, s_pickupLegend[i].g, s_pickupLegend[i].b, 255));
		M_Text(s_pickupLegend[i].text, rx + 28.0f * s_k, y, 25.0f * s_k, CP_Color_Create(220, 220, 220, 255), false);
	}
	M_Text("WASD - move     Mouse - aim     Hold LEFT CLICK - shoot     1-5 / Q / E - switch bullet     P - pause",
		s_menuW * 0.5f, s_menuH - 40.0f * s_k, 24.0f * s_k, CP_Color_Create(180, 180, 180, 255), true);
}

void Main_Menu_Exit(void)
{

}
