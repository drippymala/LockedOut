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
#include "howtoplay.h"
#include "credits.h"
#include "practicerange.h"   // TEMP playtest stage (secret code on this menu). Remove with the block in Main_Menu_Update.


CP_Font myFont;
CP_Image myImage;
CP_Sound myMusic = NULL;
static bool s_musicPlaying = false;   // so the menu music isn't restarted every time we come back from How To Play / Credits

// Loads the game font if it isn't loaded yet, then makes it the active font.
void Font_EnsureLoaded(void)
{
	if (!myFont)
	{
		myFont = CP_Font_Load("Assets/Exo2-Regular.ttf");
	}
	CP_Font_Set(myFont);
}

typedef struct { float x, y, w, h; const char* label; } MenuButton;

static MenuButton s_playButton, s_howButton, s_creditsButton, s_quitButton;
static float      s_menuW = 1600.0f, s_menuH = 900.0f;
static float      s_menuTime;
static float      s_k = 1.0f;     // UI scale: 1.0 at 1600x900, smaller for smaller windows

// Set to 1 to show the guns / pickups legend under the buttons again (everything then sits closer together).
// It is 0 by default: the same info is on the HOW TO PLAY page, and the menu gets room to breathe.
#ifndef SHOW_MENU_LEGEND
#define SHOW_MENU_LEGEND   0
#endif

// Menu layout at 1600x900 (everything is multiplied by s_k for other window sizes)
#if SHOW_MENU_LEGEND
#define MENU_TITLE_Y        130.0f
#define MENU_TAGLINE_Y      225.0f
#define MENU_BUTTON_W       380.0f
#define MENU_BUTTON_H        64.0f
#define MENU_BUTTON_Y       262.0f   // top of the first button
#define MENU_BUTTON_PITCH    76.0f   // distance from one button's top to the next
#else
#define MENU_TITLE_Y        160.0f
#define MENU_TAGLINE_Y      275.0f
#define MENU_BUTTON_W       420.0f
#define MENU_BUTTON_H        76.0f
#define MENU_BUTTON_Y       360.0f
#define MENU_BUTTON_PITCH   112.0f
#endif

#if SHOW_MENU_LEGEND
typedef struct { int r, g, b; const char* text; } LegendLine;

// The guns from weapon.h, in key order. Colours match each gun's colour in game
// (Last Stand changes colour with your HP, so it shows its mid-health red here).
static const LegendLine s_bulletLegend[] =
{
	{ 255, 230, 120, "1  PISTOL  - flat cost per shot" },
	{ 255,  50,  50, "2  HEALTH LASER  - your health bar fires as a beam" },
	{ 200,  20,  50, "3  BLOODRANG  - catch it to get your HP back" },
	{ 230,  40, 110, "4  BLOOD BOMB  - hold to pour HP in, then throw" },
	{ 228,  95,  95, "5  LAST STAND  - stronger the lower your HP" }
};

static const LegendLine s_pickupLegend[] =
{
	{ 220,  40,  60, "Blood drop	- small heal (flies to you)" },
	{ 240, 240, 240, "Medkit     - instant heal" },
	{ 235, 200, 150, "Bandage    - heal over time" },
	{ 200,  80, 240, "Bloodlust	- kills heal you for 12s" },
	{ 255, 255, 255, "Landmine	 - place explosive triggers" }
};

#endif   // SHOW_MENU_LEGEND (legend data)

/* ---- small draw helpers ---- */
#if SHOW_MENU_LEGEND
static void M_Circle(float x, float y, float radius, CP_Color c)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawCircle(x, y, radius * 2.0f);      // diameter
}

#endif   // SHOW_MENU_LEGEND (M_Circle is only used by the legend)

static void M_Rect(float x, float y, float w, float h, CP_Color c)
{
	CP_Settings_RectMode(CP_POSITION_CORNER);
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
	// load the music only once (decoding an mp3 takes a moment) and don't restart it if it is already playing
	if (!myMusic) myMusic = CP_Sound_Load("Assets/Sounds/Menu.mp3");
	if (!s_musicPlaying)
	{
		CP_Sound_PlayAdvanced(myMusic, 0.1f, 1.0f, false, CP_SOUND_GROUP_0);
		s_musicPlaying = true;
	}

	s_menuW = (float)CP_System_GetWindowWidth();
	s_menuH = (float)CP_System_GetWindowHeight();
	s_menuTime = 0.0f;
	s_k = (s_menuW / 1600.0f < s_menuH / 900.0f) ? s_menuW / 1600.0f : s_menuH / 900.0f;

	s_playButton.w = MENU_BUTTON_W * s_k;
	s_playButton.h = MENU_BUTTON_H * s_k;
	s_playButton.x = (s_menuW - s_playButton.w) * 0.5f;
	s_playButton.y = (MENU_BUTTON_Y + MENU_BUTTON_PITCH * 0.0f) * s_k;
	s_playButton.label = "PLAY";

	s_howButton = s_playButton;
	s_howButton.y = (MENU_BUTTON_Y + MENU_BUTTON_PITCH * 1.0f) * s_k;
	s_howButton.label = "HOW TO PLAY";

	s_creditsButton = s_playButton;
	s_creditsButton.y = (MENU_BUTTON_Y + MENU_BUTTON_PITCH * 2.0f) * s_k;
	s_creditsButton.label = "CREDITS";

	s_quitButton = s_playButton;
	s_quitButton.y = (MENU_BUTTON_Y + MENU_BUTTON_PITCH * 3.0f) * s_k;
	s_quitButton.label = "QUIT";
}

void Main_Menu_Update(void)
{

	float dt = CP_System_GetDt();
	if (dt > 0.05f) dt = 0.05f;
	s_menuTime += dt;

	// TEMP: secret code opens the Practice Range (practicerange.h). Delete this block to remove it.
	if (PracticeRange_CheckCode(dt))
	{
		CP_Sound_StopGroup(CP_SOUND_GROUP_0);
		s_musicPlaying = false;
		return;
	}

	float mx = CP_Input_GetMouseX();
	float my = CP_Input_GetMouseY();
	bool click = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);

	if ((click && M_ButtonHovered(&s_playButton, mx, my)) || CP_Input_KeyTriggered(KEY_ENTER) || CP_Input_KeyTriggered(KEY_SPACE))
	{
		CP_Sound_StopGroup(CP_SOUND_GROUP_0);   // the menu music stops when the game starts
		s_musicPlaying = false;
		CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
		return;
	}
	if (click && M_ButtonHovered(&s_howButton, mx, my))
	{
		CP_Engine_SetNextGameState(HowToPlay_Init, HowToPlay_Update, HowToPlay_Exit);
		return;
	}
	if (click && M_ButtonHovered(&s_creditsButton, mx, my))
	{
		CP_Engine_SetNextGameState(Credits_Init, Credits_Update, Credits_Exit);
		return;
	}
	if (click && M_ButtonHovered(&s_quitButton, mx, my))
	{
		CP_Engine_Terminate();
		return;
	}

	// ---- draw ----
	CP_Graphics_ClearBackground(CP_Color_Create(18, 22, 16, 255));

	// title (gently pulsing)
	float pulse = 0.5f + 0.5f * sinf(s_menuTime * 2.5f);
	M_Text("BLEED OUT", s_menuW * 0.5f + 4.0f * s_k, (MENU_TITLE_Y + 4.0f) * s_k, 130.0f * s_k, CP_Color_Create(0, 0, 0, 200), true);
	M_Text("BLEED OUT", s_menuW * 0.5f, MENU_TITLE_Y * s_k, 130.0f * s_k, CP_Color_Create(200 + (int)(55.0f * pulse), 30, 30, 255), true);
	M_Text("Every bullet costs your blood.", s_menuW * 0.5f, MENU_TAGLINE_Y * s_k, 34.0f * s_k, CP_Color_Create(230, 230, 230, 255), true);

	M_DrawButton(&s_playButton, mx, my);
	M_DrawButton(&s_howButton, mx, my);
	M_DrawButton(&s_creditsButton, mx, my);
	M_DrawButton(&s_quitButton, mx, my);

#if SHOW_MENU_LEGEND
	// how to play
	float lx = s_menuW * 0.5f - 640.0f * s_k;
	float rx = s_menuW * 0.5f + 80.0f * s_k;
	float ty = 590.0f * s_k;
	M_Text("GUNS (rare drops from zombies)", lx, ty, 28.0f * s_k, CP_Color_Create(255, 255, 255, 255), false);
	M_Text("PICKUPS (from enemies drops)", rx, ty, 28.0f * s_k, CP_Color_Create(255, 255, 255, 255), false);
	for (int i = 0; i < 5; ++i)
	{
		float y = ty + (38.0f + 34.0f * (float)i) * s_k;
		M_Circle(lx + 8.0f * s_k, y, 8.0f * s_k, CP_Color_Create(s_bulletLegend[i].r, s_bulletLegend[i].g, s_bulletLegend[i].b, 255));
		M_Text(s_bulletLegend[i].text, lx + 28.0f * s_k, y, 25.0f * s_k, CP_Color_Create(220, 220, 220, 255), false);
		M_Circle(rx + 8.0f * s_k, y, 8.0f * s_k, CP_Color_Create(s_pickupLegend[i].r, s_pickupLegend[i].g, s_pickupLegend[i].b, 255));
		M_Text(s_pickupLegend[i].text, rx + 28.0f * s_k, y, 25.0f * s_k, CP_Color_Create(220, 220, 220, 255), false);
	}
#endif   // SHOW_MENU_LEGEND

	M_Text("WASD - Move     Mouse - Aim     Hold LEFT CLICK - Shoot     1-5 / Q / E - Switch gun     ESC - Pause",
		s_menuW * 0.5f, s_menuH - 40.0f * s_k, 24.0f * s_k, CP_Color_Create(180, 180, 180, 255), true);
}

void Main_Menu_Exit(void)
{
	// Nothing is freed here on purpose: the music and the font stay loaded, so coming back to the menu
	// (from How To Play, Credits or the game) doesn't have to load them again. Loading them every time
	// is what made the BACK button lag. The music is stopped when PLAY is pressed (see Main_Menu_Update).
}
