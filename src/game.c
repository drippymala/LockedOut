//---------------------------------------------------------
// file:    game.c
// brief:   Game state implementation for top-down zombies
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"
#include "game.h"
#include "mainmenu.h"
#include <stdbool.h>

#define MAX_ZOMBIES 128

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;

typedef struct
{
	bool active;
	int id;
	ZombieType type;
	CP_Vector pos;
	float radius;       // Head radius
	float hp, maxHp;
	float speed, damage;
	float attackTimer, swayPhase, swayAmp, hitFlash;
} Zombie;

static Zombie s_zombies[MAX_ZOMBIES];
static CP_Color s_zombieColor;

/* ---------- Drawing Helpers ---------- */
static CP_Color Col(int r, int g, int b, int a)
{
	return CP_Color_Create(r, g, b, a);
}

static void G_Circle(float x, float y, float radius, CP_Color c)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawCircle(x, y, radius * 2.0f);
}

static void G_Ring(float x, float y, float radius, float weight, CP_Color c)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoFill();
	CP_Settings_Stroke(c);
	CP_Settings_StrokeWeight(weight);
	CP_Graphics_DrawCircle(x, y, radius * 2.0f);
	CP_Settings_NoStroke();
}

static void G_Rect(float x, float y, float w, float h, CP_Color c)
{
	CP_Settings_RectMode(CP_POSITION_CORNER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawRect(x, y, w, h);
}

/* ---------- Zombie Drawing (Head + Extended Rectangular Arms) ---------- */
static void DrawZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float x = z->pos.x;
		float y = z->pos.y;
		float r = z->radius;

		CP_Color skinColor;
		float armW, armL, armOffsetY;

		// 1. Dimensions and colors based on archetype
		if (z->type == Z_WALKER)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(95, 155, 85, 255);
			armW = 8.0f;
			armL = r * 1.5f;
			armOffsetY = 0.0f;
		}
		else if (z->type == Z_RUNNER)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(220, 180, 50, 255);
			armW = 6.0f;
			armL = r * 1.8f;
			armOffsetY = -r * 0.2f;
		}
		else // Z_BRUTE
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(55, 95, 55, 255);
			armW = 16.0f;
			armL = r * 1.4f;
			armOffsetY = r * 0.2f;
		}

		// 2. Rectangular Arms reaching upward/forward
		G_Rect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);
		G_Rect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);

		// Arm outlines
		CP_Settings_RectMode(CP_POSITION_CORNER);
		CP_Settings_NoFill();
		CP_Settings_Stroke(Col(20, 35, 20, 255));
		CP_Settings_StrokeWeight(1.5f);
		CP_Graphics_DrawRect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Graphics_DrawRect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Settings_NoStroke();

		// 3. Circular Head
		G_Circle(x, y, r, skinColor);
		G_Ring(x, y, r, 2.0f, Col(20, 35, 20, 255));

		// 4. Floating Health Bar (only if damaged)
		if (z->hp < z->maxHp)
		{
			float barW = r * 2.4f;
			float hpRatio = z->hp / z->maxHp;
			float barY = y - r - armL - 10.0f;

			G_Rect(x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));
			G_Rect(x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));
		}
	}
}

/* ---------- State Functions ---------- */
void Game_Init(void)
{
	s_zombieColor = Col(95, 155, 85, 255);

	// Reset all zombie slots
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
}

void Game_Update(void)
{
	// Press Q to return to the Main Menu
	if (CP_Input_KeyTriggered(KEY_Q))
	{
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
		return;
	}

	CP_Graphics_ClearBackground(Col(24, 28, 24, 255));

	// Draw active game zombies
	DrawZombies();
}

void Game_Exit(void)
{
}