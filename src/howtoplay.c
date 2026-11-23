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
	{ C_BODY, HS_BODY, "Q / E  -  previous / next gun you have unlocked (it wraps around)" },
	{ C_BODY, HS_BODY, "ESC  -  pause / resume          M (while paused)  -  back to the main menu" },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "THE ONE RULE: EVERY SHOT COSTS YOUR BLOOD" },
	{ C_BODY, HS_BODY, "Your health bar is your ammo. Every shot you fire takes HP away from you." },
	{ C_BODY, HS_BODY, "Guns now fire one last time and leaves you at 1 HP. At 1 HP your guns stop firing." },
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
	{ C_BODY, HS_BODY, "Zombies get tougher each wave: more health, faster movement and harder bites. Later waves spawn faster." },
	{ C_BODY, HS_BODY, "Waves 5, 10 and 15 bring a boss early in the wave. Kill it to pick a reward: a Pistol upgrade, or full heal + 15% max HP." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "ZOMBIE TYPES" },
	{ 110, 190, 100, HS_BODY, "WALKER  -  average speed and health. Shows up from wave 1." },
	{ 220, 180,  50, HS_BODY, "RUNNER  -  fast but fragile. Starts appearing at wave 3." },
	{  90, 150,  90, HS_BODY, "BRUTE  -  big, slow and very tanky, and it bites hard. Starts appearing at wave 5." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_HEAD, "WHEN YOU CLEAR A WAVE" },
	{ C_BODY, HS_BODY, "You get +5 max HP and a +25 HP heal." },
	{ C_BODY, HS_BODY, "After every 3rd wave (3, 6, 9, 12, 15 and 18) the game freezes and you pick ONE card:" },
	{ C_BODY, HS_BODY, "     UPGRADE  -  raise a gun you own by one level (up to Lv4, or Lv5 for the Pistol)." },
	{ C_BODY, HS_BODY, "     NEW GUN  -  unlock a gun you don't have yet, starting at Lv1." },
	{ C_BODY, HS_BODY, "A gun's last level is its EVOLVED form (Pistol Lv5, the others Lv4) and gives it an extra ability." },
	{ 0, 0, 0, HS_GAP, "" },
	{ 220, 150, 255, HS_HEAD, "WAVE 20  -  THE BOSS" },
	{ C_BODY, HS_BODY, "THE TYRANT has 12000 HP and cycles through a dash, a volley of shots and a pull. Kill it to win." },
	{ C_BODY, HS_BODY, "At half HP it summons a ring of walkers and runners every 10s. At 25% HP it enrages: redder and faster." }
};

static const HowLine s_gunsPage[] =
{
	{ C_HEAD, HS_HEAD, "THE 5 GUNS  (all of them run on your HP)" },
	{ C_BODY, HS_BODY, "You start with the Pistol. The other four are unlocked from the cards every 3 waves." },
	{ 0, 0, 0, HS_GAP, "" },
	{ 255, 230, 120, HS_BODY, "1  PISTOL  -  cheap flat HP cost per shot, fast bullets." },
	{ C_DIM, HS_BODY, "     Lv5 TWIN FANG: fires a wide spread of 3 bullets that each go through a zombie, and hits can steal life." },
	{ 255,  50,  50, HS_BODY, "2  HEALTH LASER  -  fires your health bar as a beam. Costs 8 HP + 10% of your current HP, and pierces everything." },
	{ C_DIM, HS_BODY, "     The more HP you have, the more damage. You can't fire again until the beam leaves the screen." },
	{ C_DIM, HS_BODY, "     Lv4 LIFELINE LANCE: much stronger, and every zombie it hits heals you." },
	{ 200,  20,  50, HS_BODY, "3  BLOODRANG  -  throw a boomerang that holds your HP. Catch it to get the HP back." },
	{ C_DIM, HS_BODY, "     Every zombie it hits boosts your next throws. Miss it and it bleeds HP every lap." },
	{ C_DIM, HS_BODY, "     Lv4 TWIN RANG: throws two at once on arcs that cross at your aim, and the boost can stack up to +80%." },
	{ 230,  40, 110, HS_BODY, "4  BLOOD BOMB  -  hold LEFT CLICK to pour HP into an orb, release to throw it." },
	{ C_DIM, HS_BODY, "     More HP inside = a bigger, harder blast. Most zombies it kills drop a blood heal." },
	{ C_DIM, HS_BODY, "     Lv4 BLOOD CLUSTER: 4 mini bombs burst inside the blast, and it leaves a pool that hurts zombies in it." },
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

// What changed since the previous version. Smaller text so it all fits on one page.
#define HS_NOTE_HEAD  26.0f
#define HS_NOTE       19.0f
static const HowLine s_patchPage[] =
{
	{ C_HEAD, HS_HEAD, "PATCH NOTES  -  what changed since the last version" },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_NOTE_HEAD, "GAMEPLAY" },
	{ C_BODY, HS_NOTE, "Wave 5 / 10 / 15 bosses arrive once 25% of the wave is out. Kill one to pick: a Pistol upgrade, or full heal + 15% max HP." },
	{ C_BODY, HS_NOTE, "Every gun fires one last time and leaves you at 1 HP. Clicking an upgrade card no longer fires your gun." },
	{ C_BODY, HS_NOTE, "The main menu music now loops instead of stopping when it ends." },
	{ 0, 0, 0, HS_GAP, "" },
	{ 220, 150, 255, HS_NOTE_HEAD, "BOSSES" },
	{ C_BODY, HS_NOTE, "The Tyrant: 2000 -> 12000 HP, and below half HP it summons a ring of walkers and runners every 10s." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_NOTE_HEAD, "GUNS" },
	{ 255, 230, 120, HS_NOTE, "PISTOL: a new level (5 in total). Damage 10 / 16 / 21 / 28 and slightly faster at the top. Lv5 is now TWIN FANG (33 damage)." },
	{ C_DIM, HS_NOTE, "     Twin Fang: much wider spread, the zombie behind a pierced one takes only 35% damage (was 100%), and hits have a 5% chance to heal 5 HP." },
	{ 255,  50,  50, HS_NOTE, "HEALTH LASER: costs 8 HP + 10% of your HP (was 25%). The beam flies faster, so it fires more often. Each zombie after the first takes 10% less." },
	{ 200,  20,  50, HS_NOTE, "BLOODRANG: flies out and back about 30% faster, bigger at Lv3/Lv4. Lv3 refund 100% -> 80%. Boost cap +50% -> +40% (Twin Rang: +100% -> +80%)." },
	{ C_DIM, HS_NOTE, "     Twin Rang: both boomerangs now curve out and cross exactly where you aim." },
	{ 230,  40, 110, HS_NOTE, "BLOOD BOMB: hits harder, bigger blast, charges faster, costs 75% of what you pour in. The orb holds up to 40% of MAX HP (was current HP)." },
	{ C_DIM, HS_NOTE, "     0.2s between throws (was 0.5s). A direct hit does +50%. Kills drop a heal 80% of the time (was 20-60%). Charge sound fits the orb size." },
	{ C_DIM, HS_NOTE, "     Blood Cluster: holds up to 60% of max HP, weaker mini bombs burst inside the blast, and it leaves a blood pool for 2s." },
	{ 228,  95,  95, HS_NOTE, "LAST STAND: max damage 30 / 52 / 90 at Lv1-3 (was 38 / 68 / 120), fires a bit slower, slower bullets, pays off earlier in the red." },
	{ C_DIM, HS_NOTE, "     Death's Door: each zombie a bullet goes through halves its damage. Free shots only once you are at 1 HP." }
};

// Which gun is strongest at each level. The Pistol has an extra level, so its Lv2-3 sit with the others' Lv2,
// its Lv4 with their Lv3, and Twin Fang (Lv5) with their evolved Lv4. Last Stand is rated at low HP.
#define C_TIER_S  255, 210,  80
#define C_TIER_A  140, 220, 120
#define C_TIER_B  120, 180, 240
#define C_TIER_C  170, 170, 170
static const HowLine s_tierPage[] =
{
	{ C_HEAD, HS_HEAD, "TIER LIST  -  the strongest guns at each level" },
	{ C_DIM, HS_NOTE, "Pistol levels are lined up with the others: its Lv2-3 count as Lv2, Lv4 as Lv3, and Twin Fang (Lv5) as Lv4." },
	{ C_DIM, HS_NOTE, "Last Stand is rated at low HP - at full HP it is the weakest gun." },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_NOTE_HEAD, "SINGLE TARGET  (bosses, brutes)" },
	{ C_TIER_S, HS_NOTE, "S     Lv3: Health Laser, Pistol          Lv4: Bloodrang, Health Laser, Last Stand" },
	{ C_TIER_A, HS_NOTE, "A     Lv1: Health Laser     Lv2: Health Laser, Pistol, Bloodrang, Blood Bomb     Lv3: Bloodrang, Blood Bomb, Last Stand     Lv4: Pistol, Blood Bomb" },
	{ C_TIER_B, HS_NOTE, "B     Lv1: Blood Bomb, Bloodrang, Last Stand     Lv2: Last Stand" },
	{ C_TIER_C, HS_NOTE, "C     Lv1: Pistol" },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_NOTE_HEAD, "CROWDS  (packs of zombies)" },
	{ C_TIER_S, HS_NOTE, "S     Lv1-3: Health Laser, Blood Bomb          Lv4: Bloodrang, Health Laser, Blood Bomb" },
	{ C_TIER_A, HS_NOTE, "A     Lv2-3: Bloodrang          Lv4: Last Stand" },
	{ C_TIER_B, HS_NOTE, "B     Lv1: Bloodrang          Lv4: Pistol" },
	{ C_TIER_C, HS_NOTE, "C     Lv1-3: Pistol, Last Stand" },
	{ 0, 0, 0, HS_GAP, "" },
	{ C_HEAD, HS_NOTE_HEAD, "QUICK PICKS" },
	{ C_BODY, HS_NOTE, "Early on, the Health Laser is the safest pick for everything. The Blood Bomb is just as good against crowds." },
	{ C_BODY, HS_NOTE, "Fully evolved, Twin Rang and Lifeline Lance are top in both. Death's Door hits hardest of all - if you dare stay near 1 HP." }
};

#define HOW_COUNT(a)  ((int)(sizeof(a) / sizeof((a)[0])))
static const HowPage s_pages[] =
{
	{ "CONTROLS & GOAL",  s_controlsPage, HOW_COUNT(s_controlsPage) },
	{ "WAVES",            s_wavesPage,    HOW_COUNT(s_wavesPage)    },
	{ "GUNS",             s_gunsPage,     HOW_COUNT(s_gunsPage)     },
	{ "PICKUPS & SCORE",  s_pickupsPage,  HOW_COUNT(s_pickupsPage)  },
	{ "PATCH NOTES",      s_patchPage,    HOW_COUNT(s_patchPage)    },
	{ "TIER LIST",        s_tierPage,     HOW_COUNT(s_tierPage)     }
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
	G_Text(b->label, b->x + b->w * 0.5f, b->y + b->h * 0.5f, 23.0f * s_k, text, AL_CENTER);   // smaller so 6 tabs fit
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
	float tabW = 235.0f * s_k;   // 6 tabs must fit across the screen
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
	if (CP_Input_KeyTriggered(KEY_5)) s_page = 4;
	if (CP_Input_KeyTriggered(KEY_6)) s_page = 5;
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
