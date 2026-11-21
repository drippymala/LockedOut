//---------------------------------------------------------
// file:	howtoplay.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	How to play game state. A few pages explaining the
//			controls, the waves, the guns and the pickups.
//			Opened from the main menu, and goes back to it.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>

#include "cprocessing.h"
#include "utils.h"        // Col, G_Rect, G_Text
#include "mainmenu.h"     // Font_EnsureLoaded, myFont, Main_Menu_* (to go back)
#include "howtoplay.h"

/* =========================================================
   The pages
   Each page is a list of lines: colour, text size and text.
   A size of 0 is just a small gap. Add / edit lines here.
   ========================================================= */
typedef struct { int r, g, b; float size; const char* text; } HowLine;
typedef struct { const char* title; const HowLine* lines; int count; } HowPage;

#define HS_HEAD   32.0f     // heading text size
#define HS_BODY   24.0f     // normal text size
#define HS_GAP    0.0f      // small gap
#define C_HEAD    255, 230, 120
#define C_BODY    225, 225, 225
#define C_DIM     165, 165, 165

static const HowLine s_controlsPage[] =
{
	{ C_HEAD, HS_HEAD, "CONTROLS" },
	{ C_BODY, HS_BODY, "W A S D  -  move" },
	{ C_BODY, HS_BODY, "Mouse  -  aim (your gun always points at the cursor)" },
	{ C_BODY, HS_BODY, "Hold LEFT CLICK  -  shoot / use your current gun" },
	{ C_BODY, HS_BODY, "1 - 5  -  switch to a gun you have unlocked (you start with only the Pistol)" },
	{ C_BODY, HS_BODY, "ESC  -  pause / resume          M (while paused)  -  back to the main menu" },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "THE ONE RULE: EVERY SHOT COSTS YOUR BLOOD" },
	{ C_BODY, HS_BODY, "Your health bar is your ammo. Every shot you fire takes HP away from you." },
	{ C_BODY, HS_BODY, "A gun refuses to fire if the shot would drop you below 1 HP, and your health bar flashes red." },
	{ C_BODY, HS_BODY, "Zombies bite when they touch you, so keep moving and keep killing to stay alive." },
	{ C_BODY, HS_BODY, "Walk over the pickups zombies drop to heal back up." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "THE GOAL" },
	{ C_BODY, HS_BODY, "Survive all 20 waves. Wave 20 is a boss fight - kill the boss to escape and win." },
	{ C_BODY, HS_BODY, "If your HP reaches 0 the run is over. Your best wave and score this session show on the main menu." }
};

static const HowLine s_wavesPage[] =
{
	{ C_HEAD, HS_HEAD, "20 WAVES" },
	{ C_BODY, HS_BODY, "Wave N spawns 4 + 4 x N zombies (wave 1 = 8, wave 10 = 44, wave 19 = 80), from every edge of the screen." },
	{ C_BODY, HS_BODY, "Zombies get tougher each wave: more health, faster movement and harder bites." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "ZOMBIE TYPES" },
	{ 110, 190, 100, HS_BODY, "WALKER  -  average speed and health. Shows up from wave 1." },
	{ 220, 180,  50, HS_BODY, "RUNNER  -  fast but fragile. Starts appearing at wave 3." },
	{  90, 150,  90, HS_BODY, "BRUTE  -  big, slow and very tanky, and it bites hard. Starts appearing at wave 5." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "WHEN YOU CLEAR A WAVE" },
	{ C_BODY, HS_BODY, "You get +5 max HP and a +25 HP heal." },
	{ C_BODY, HS_BODY, "After every 3rd wave (3, 6, 9, 12, 15 and 18) the game freezes and you pick ONE card:" },
	{ C_BODY, HS_BODY, "     UPGRADE  -  raise a gun you own by one level (Lv1 up to Lv4)." },
	{ C_BODY, HS_BODY, "     NEW GUN  -  unlock a gun you don't have yet, starting at Lv1." },
	{ C_BODY, HS_BODY, "Lv4 is the EVOLVED form of a gun and gives it an extra ability." },
	{ 0, 0, 0, HS_GAP, "" },
	{ 220, 150, 255, HS_HEAD, "WAVE 20  -  THE BOSS" },
	{ C_BODY, HS_BODY, "PATIENT ZERO is a huge zombie with 9000 HP. It is the only enemy on wave 20 - kill it to win." }
};

static const HowLine s_gunsPage[] =
{
	{ C_HEAD, HS_HEAD, "THE 5 GUNS  (all of them run on your HP)" },
	{ C_BODY, HS_BODY, "You start with the Pistol. The other four are unlocked from the cards every 3 waves." },
	{ 0, 0, 0, HS_GAP, "" },
	{ 255, 230, 120, HS_BODY, "1  PISTOL  -  cheap flat HP cost per shot, fast bullets." },
	{ C_DIM, HS_BODY, "     Lv4 TWIN FANG: fires a spread of 3 bullets, and each one goes through a zombie." },
	{ 255,  50,  50, HS_BODY, "2  HEALTH LASER  -  fires your health bar as a beam. Costs 25% of your current HP and pierces everything." },
	{ C_DIM, HS_BODY, "     The more HP you have, the more damage. You can't fire again until the beam leaves the screen." },
	{ C_DIM, HS_BODY, "     Lv4 LIFELINE LANCE: much stronger, and every zombie it hits heals you." },
	{ 200,  20,  50, HS_BODY, "3  BLOODRANG  -  throw a boomerang that holds your HP. Catch it to get the HP back." },
	{ C_DIM, HS_BODY, "     Every zombie it hits boosts your next throws. Miss it and it bleeds HP every lap." },
	{ C_DIM, HS_BODY, "     Lv4 TWIN RANG: throws two at once, and the boost can stack twice as high." },
	{ 230,  40, 110, HS_BODY, "4  BLOOD BOMB  -  hold LEFT CLICK to pour HP into an orb, release to throw it." },
	{ C_DIM, HS_BODY, "     More HP inside = a bigger blast and more damage. You move slower while charging." },
	{ C_DIM, HS_BODY, "     Lv4 BLOOD CLUSTER: the blast throws out 4 mini bombs that explode again." },
	{ 228,  95,  95, HS_BODY, "5  LAST STAND  -  a revolver (3 HP per shot) that hits harder the LOWER your HP is." },
	{ C_DIM, HS_BODY, "     Lv4 DEATH'S DOOR: much higher damage near death, and its bullets go through zombies." }
};

static const HowLine s_pickupsPage[] =
{
	{ C_HEAD, HS_HEAD, "PICKUPS  (dropped by zombies - walk over them)" },
	{ 220,  40,  60, HS_BODY, "BLOOD DROP  -  heals 4 HP. Common, and it flies to you when you are close." },
	{ 240, 240, 240, HS_BODY, "MEDKIT  -  instantly heals 30 HP. Rare." },
	{ 235, 200, 150, HS_BODY, "BANDAGE  -  heals 40 HP over time (6 HP per second). Rare." },
	{ 200,  80, 240, HS_BODY, "BLOODLUST  -  for 12 seconds, every kill heals you 5 HP. Rare." },
	{ C_DIM, HS_BODY, "Drops get more likely when you are low on HP, and brutes often drop an extra medkit or bandage." },
	{ C_DIM, HS_BODY, "Pickups blink and then vanish after about 10-14 seconds, so grab them quickly." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "SCORE" },
	{ C_BODY, HS_BODY, "Walker 10        Runner 15        Brute 50        Boss 2000" },
	{ C_BODY, HS_BODY, "Clearing a wave also gives 100 x the wave number." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "TIPS" },
	{ C_BODY, HS_BODY, "Your health is your ammo - don't waste shots on zombies that are still far away." },
	{ C_BODY, HS_BODY, "Nearly dead? Switch to LAST STAND: it hits the hardest when you are close to 0 HP." }
};

#define HOW_COUNT(a)  ((int)(sizeof(a) / sizeof((a)[0])))
static const HowPage s_pages[] =
{
	{ "CONTROLS & GOAL",  s_controlsPage, HOW_COUNT(s_controlsPage) },
	{ "WAVES",            s_wavesPage,    HOW_COUNT(s_wavesPage)    },
	{ "GUNS",             s_gunsPage,     HOW_COUNT(s_gunsPage)     },
	{ "PICKUPS & SCORE",  s_pickupsPage,  HOW_COUNT(s_pickupsPage)  }
};
#define HOW_PAGE_COUNT  HOW_COUNT(s_pages)

/* =========================================================
   State data
   ========================================================= */
typedef struct { float x, y, w, h; const char* label; } HowButton;

static HowButton s_backButton;                 // BACK, top-left
static HowButton s_tabs[HOW_PAGE_COUNT];       // one tab per section, along the top
static float     s_w = 1600.0f, s_h = 900.0f;  // window size
static float     s_k = 1.0f;                   // UI scale: 1.0 at 1600x900, smaller for smaller windows
static int       s_page = 0;                   // which section is showing (0-based)

/* ---- buttons and tabs ---- */
static bool HowButton_Hovered(const HowButton* b, float mx, float my)
{
	return mx >= b->x && mx <= b->x + b->w && my >= b->y && my <= b->y + b->h;
}

// Same look as the main menu buttons.
static void HowButton_Draw(const HowButton* b, float mx, float my, float textSize)
{
	bool hover = HowButton_Hovered(b, mx, my);

	G_Rect(b->x - 4.0f * s_k, b->y - 4.0f * s_k, b->w + 8.0f * s_k, b->h + 8.0f * s_k,
		hover ? Col(255, 230, 120, 255) : Col(90, 20, 20, 255));
	G_Rect(b->x, b->y, b->w, b->h,
		hover ? Col(170, 30, 30, 255) : Col(60, 12, 12, 255));
	G_Text(b->label, b->x + b->w * 0.5f, b->y + b->h * 0.5f, textSize * s_k, Col(255, 255, 255, 255), AL_CENTER);
}

// A tab: the selected one has a yellow border and a bright red fill, the others are darker.
static void HowTab_Draw(const HowButton* b, bool selected, float mx, float my)
{
	bool hover = HowButton_Hovered(b, mx, my);

	CP_Color border = selected ? Col(255, 230, 120, 255) : (hover ? Col(200, 160, 80, 255) : Col(90, 20, 20, 255));
	CP_Color fill = selected ? Col(170, 30, 30, 255) : (hover ? Col(110, 22, 22, 255) : Col(45, 10, 10, 255));
	CP_Color text = selected ? Col(255, 255, 255, 255) : Col(200, 200, 200, 255);

	G_Rect(b->x - 4.0f * s_k, b->y - 4.0f * s_k, b->w + 8.0f * s_k, b->h + 8.0f * s_k, border);
	G_Rect(b->x, b->y, b->w, b->h, fill);
	G_Text(b->label, b->x + b->w * 0.5f, b->y + b->h * 0.5f, 26.0f * s_k, text, AL_CENTER);
}

/* ---- drawing one section ---- */
static void DrawPage(const HowPage* page)
{
	float x = s_w * 0.5f - 700.0f * s_k;
	float y = 195.0f * s_k;

	for (int i = 0; i < page->count; ++i)
	{
		const HowLine* line = &page->lines[i];
		if (line->size <= 0.0f)
		{
			y += 14.0f * s_k;   // gap
			continue;
		}
		float lineHeight = line->size * 1.35f * s_k;
		G_Text(line->text, x, y + lineHeight * 0.5f, line->size * s_k, Col(line->r, line->g, line->b, 255), AL_LEFT);
		y += lineHeight;
	}
}

static void GoBackToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   Game state callbacks
   ========================================================= */
void HowToPlay_Init(void)
{
	Font_EnsureLoaded();   // the menu frees its font on exit, so load it again for this screen

	s_w = (float)CP_System_GetWindowWidth();
	s_h = (float)CP_System_GetWindowHeight();
	s_k = (s_w / 1600.0f < s_h / 900.0f) ? s_w / 1600.0f : s_h / 900.0f;
	s_page = 0;

	// BACK button, top-left
	s_backButton = (HowButton){ 34.0f * s_k, 24.0f * s_k, 170.0f * s_k, 50.0f * s_k, "BACK" };

	// one tab per section, in a row under the title
	float tabW = 340.0f * s_k;
	float tabH = 50.0f * s_k;
	float gap = 20.0f * s_k;
	float startX = (s_w - (tabW * (float)HOW_PAGE_COUNT + gap * (float)(HOW_PAGE_COUNT - 1))) * 0.5f;
	for (int i = 0; i < HOW_PAGE_COUNT; ++i)
	{
		s_tabs[i] = (HowButton){ startX + (tabW + gap) * (float)i, 100.0f * s_k, tabW, tabH, s_pages[i].title };
	}
}

void HowToPlay_Update(void)
{
	float mx = CP_Input_GetMouseX();
	float my = CP_Input_GetMouseY();
	bool click = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);

	// ---- input ----
	if (CP_Input_KeyTriggered(KEY_ESCAPE) || (click && HowButton_Hovered(&s_backButton, mx, my)))
	{
		GoBackToMenu();
		return;
	}

	// click a tab to open that section
	if (click)
	{
		for (int i = 0; i < HOW_PAGE_COUNT; ++i)
		{
			if (HowButton_Hovered(&s_tabs[i], mx, my)) s_page = i;
		}
	}

	// keyboard shortcuts: 1-4 jump straight to a section, LEFT / RIGHT move between them
	if (CP_Input_KeyTriggered(KEY_1)) s_page = 0;
	if (CP_Input_KeyTriggered(KEY_2)) s_page = 1;
	if (CP_Input_KeyTriggered(KEY_3)) s_page = 2;
	if (CP_Input_KeyTriggered(KEY_4)) s_page = 3;
	if (CP_Input_KeyTriggered(KEY_RIGHT)) s_page = (s_page + 1) % HOW_PAGE_COUNT;
	if (CP_Input_KeyTriggered(KEY_LEFT))  s_page = (s_page + HOW_PAGE_COUNT - 1) % HOW_PAGE_COUNT;

	// ---- draw ----
	CP_Graphics_ClearBackground(Col(18, 22, 16, 255));

	G_Text("HOW TO PLAY", s_w * 0.5f, 48.0f * s_k, 56.0f * s_k, Col(200, 30, 30, 255), AL_CENTER);
	HowButton_Draw(&s_backButton, mx, my, 30.0f);

	for (int i = 0; i < HOW_PAGE_COUNT; ++i)
	{
		HowTab_Draw(&s_tabs[i], i == s_page, mx, my);
	}

	// dark panel behind the text so it is easy to read
	G_Rect(s_w * 0.5f - 740.0f * s_k, 170.0f * s_k, 1480.0f * s_k, 690.0f * s_k, Col(0, 0, 0, 120));

	DrawPage(&s_pages[s_page]);

	G_Text("Click a tab (or press 1-4 / LEFT / RIGHT) to switch section      ESC - back to menu",
		s_w * 0.5f, s_h - 20.0f * s_k, 20.0f * s_k, Col(150, 150, 150, 255), AL_CENTER);
}

void HowToPlay_Exit(void)
{
	// the font stays loaded (see Main_Menu_Exit), so going back to the menu is instant
}
