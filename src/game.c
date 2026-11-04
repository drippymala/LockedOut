//---------------------------------------------------------
// file:    game.c
// author:  LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:   Source file containing the implementation of the 
//          game state, including initialization, game updates, and 
//          exit functions.
//
// Copyright © 2026 DigiPen, All rights reserved
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"       // Fixes C4013 'IsAreaClicked' undefined
#include "game.h"        // Matches the Game_Init / Game_Update / Game_Exit declarations
#include "mainmenu.h"
#include <math.h>
#include <stdbool.h>

#define PLAYER_RADIUS        18.0f
#define PLAYER_SPEED         290.0f
#define GUN_BARREL_LENGTH    14.0f
#define GUN_BARREL_THICKNESS 9.0f
#define GRID_CELL_SIZE       100.0f
#define MAX_DELTA_TIME       0.05f
#define MAX_ZOMBIES          128
#define MAX_BULLETS       512

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;
typedef enum { W_PISTOL, W_SHOTGUN, W_LEECH, W_MINIGUN, W_RAILGUN, W_COUNT } WeaponType;

//weapons
typedef struct
{
	const char* name;
	const char* drain;      // text describing how it drains health
	float fireDelay;        // seconds between shots
	float speed;            // bullet speed (px/s)
	float radius;
	float damage;
	float life;             // seconds before the bullet disappears
	int   pellets;
	float spreadDeg;        // total cone angle
	int   pierce;           // extra zombies a bullet can go through
	int   r, g, b;
} WeaponDef;

static const WeaponDef s_weapons[W_COUNT] =
{
	//  name       drain description               delay   speed  rad  dmg   life   pel spread pierce   colour
	{ "PISTOL",  "flat cost per shot",            0.25f,  900.0f, 5.0f, 20.0f, 1.20f, 1,  1.5f,  0,  255, 230, 120 },
	{ "SHOTGUN", "big cost per blast",            0.75f,  800.0f, 4.0f, 11.0f, 0.45f, 7, 26.0f,  0,  255, 150,  60 },
	{ "LEECH",   "pay up front, refunded on hit", 0.40f,  750.0f, 6.0f, 12.0f, 1.20f, 1,  2.0f,  0,  110, 255, 140 },
	{ "MINIGUN", "cost ramps up while held",      0.085f,1000.0f, 4.0f,  9.0f, 1.00f, 1,  5.0f,  0,  120, 200, 255 },
	{ "RAILGUN", "% of your current health",      1.10f, 2400.0f, 4.0f,120.0f, 0.80f, 1,  0.0f, 99,  220, 120, 255 }
};

typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel;
	float life, damage, radius;
	int pierceLeft;
	int hitIds[MAX_PIERCE_TRACK];
	int hitCount;
} Bullet;

typedef struct
{
	CP_Vector position;
	float     aimAngleRadians;
	float hp;       // Player's current health
	float maxHp;    // Player's maximum health
	float ghostHp;  // Health value used to display the trailing ghost bar

} Player;

typedef struct
{
	bool active;
	int id;
	ZombieType type;
	CP_Vector pos;
	float radius;
	float hp, maxHp;
	float speed, damage;
	float attackTimer, swayPhase, swayAmp, hitFlash;
} Zombie;

static Zombie   s_zombies[MAX_ZOMBIES];
static CP_Color s_zombieColor;
static float    windowWidth = 1600.0f;
static float    windowHeight = 900.0f;
static Player   player;


/* ---------- Helper Functions ---------- */
static CP_Color Col(int r, int g, int b, int a)
{
	return CP_Color_Create(r, g, b, a);
}

static CP_Vector GetNormalizedDirection(CP_Vector direction)
{
	float length = CP_Vector_Length(direction);
	if (length < 0.0001f)
	{
		return CP_Vector_Set(0.0f, 0.0f);
	}
	return CP_Vector_Normalize(direction);
}

static void DrawFilledCircle(float centerX, float centerY, float radius, CP_Color color)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(color);
	CP_Graphics_DrawCircle(centerX, centerY, radius * 2.0f);
}

static void DrawCircleOutline(float centerX, float centerY, float radius, float lineThickness, CP_Color color)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoFill();
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawCircle(centerX, centerY, radius * 2.0f);
	CP_Settings_NoStroke();
}

static void DrawColoredLine(float startX, float startY, float endX, float endY, float lineThickness, CP_Color color)
{
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawLine(startX, startY, endX, endY);
	CP_Settings_NoStroke();
}

static void G_Circle(float x, float y, float radius, CP_Color c)
{
	DrawFilledCircle(x, y, radius, c);
}

static void G_Ring(float x, float y, float radius, float weight, CP_Color c)
{
	DrawCircleOutline(x, y, radius, weight, c);
}

static void G_Rect(float x, float y, float w, float h, CP_Color c)
{
	CP_Settings_RectMode(CP_POSITION_CORNER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawRect(x, y, w, h);
}

/* ---------- Logic & Rendering ---------- */
static void UpdatePlayerMovementAndAim(float deltaTime)
{
	CP_Vector inputDirection = CP_Vector_Set(0.0f, 0.0f);

	if (CP_Input_KeyDown(KEY_W)) inputDirection.y -= 1.0f;
	if (CP_Input_KeyDown(KEY_S)) inputDirection.y += 1.0f;
	if (CP_Input_KeyDown(KEY_A)) inputDirection.x -= 1.0f;
	if (CP_Input_KeyDown(KEY_D)) inputDirection.x += 1.0f;

	CP_Vector moveDirection = GetNormalizedDirection(inputDirection);
	CP_Vector velocity = CP_Vector_Scale(moveDirection, PLAYER_SPEED * deltaTime);
	player.position = CP_Vector_Add(player.position, velocity);

	player.position.x = CP_Math_ClampFloat(player.position.x, PLAYER_RADIUS, windowWidth - PLAYER_RADIUS);
	player.position.y = CP_Math_ClampFloat(player.position.y, PLAYER_RADIUS, windowHeight - PLAYER_RADIUS);

	float mouseDeltaX = CP_Input_GetMouseX() - player.position.x;
	float mouseDeltaY = CP_Input_GetMouseY() - player.position.y;
	player.aimAngleRadians = atan2f(mouseDeltaY, mouseDeltaX);
}

static void DrawBackgroundGrid(void)
{
	CP_Color backgroundColor = Col(28, 34, 26, 255);
	CP_Color gridLineColor = Col(255, 255, 255, 10);

	CP_Graphics_ClearBackground(backgroundColor);

	for (float x = 0.0f; x <= windowWidth; x += GRID_CELL_SIZE)
	{
		DrawColoredLine(x, 0.0f, x, windowHeight, 1.0f, gridLineColor);
	}
	for (float y = 0.0f; y <= windowHeight; y += GRID_CELL_SIZE)
	{
		DrawColoredLine(0.0f, y, windowWidth, y, 1.0f, gridLineColor);
	}
}

static void DrawPlayerSprite(void)
{
	CP_Color gunColor = Col(255, 230, 120, 255);
	CP_Color bodyColor = Col(60, 130, 220, 255);
	CP_Color highlightColor = Col(120, 185, 255, 255);

	CP_Vector aimDirection = CP_Vector_Set(cosf(player.aimAngleRadians), sinf(player.aimAngleRadians));
	float     barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	CP_Vector gunTipPos = CP_Vector_Add(player.position, CP_Vector_Scale(aimDirection, barrelReach));

	DrawColoredLine(player.position.x, player.position.y, gunTipPos.x, gunTipPos.y, GUN_BARREL_THICKNESS, gunColor);
	DrawFilledCircle(player.position.x, player.position.y, PLAYER_RADIUS, bodyColor);
	DrawFilledCircle(player.position.x - 4.0f, player.position.y - 4.0f, PLAYER_RADIUS * 0.45f, highlightColor);
}

static void DrawMouseCrosshair(void)
{
	float    mouseX = CP_Input_GetMouseX();
	float    mouseY = CP_Input_GetMouseY();
	CP_Color crosshairColor = Col(255, 230, 120, 230);

	DrawCircleOutline(mouseX, mouseY, 11.0f, 2.0f, crosshairColor);
	DrawColoredLine(mouseX - 20.0f, mouseY, mouseX - 14.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX + 14.0f, mouseY, mouseX + 20.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY - 20.0f, mouseX, mouseY - 14.0f, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY + 14.0f, mouseX, mouseY + 20.0f, 2.0f, crosshairColor);
}

static void DrawHud(void)
{
	char buf[160];
	const Player* p = &player;

	// Position and dimensions of the health bar
	const float bx = 30.0f;       // X position of the health bar
	const float by = 28.0f;       // Y position of the health bar
	const float bw = 380.0f;      // Maximum width of the health bar
	const float bh = 30.0f;       // Height of the health bar

	// Calculate the percentage of the player's current health
	// ClampF ensures the value stays between 0.0 and 1.0
	float ratio = CP_Math_ClampFloat(p->hp / p->maxHp, 0.0f, 1.0f);

	// Calculate the percentage of the ghost health bar
	float ghost = CP_Math_ClampFloat(p->ghostHp / p->maxHp, 0.0f, 1.0f);

	// Change the health bar colour depending on the player's remaining health
	CP_Color hpCol = ratio > 0.5f ? Col(70, 200, 90, 255) :       // Green: Above 50% HP
		(ratio > 0.25f ? Col(230, 190, 50, 255) :   // Yellow: Above 25% HP
			Col(220, 50, 50, 255));    // Red: 25% HP or below

	// Draw the dark background behind the health bar
	G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(0, 0, 0, 170));

	// Draw the ghost health bar to show recently lost health
	G_Rect(bx, by, bw * ghost, bh, Col(255, 255, 255, 130));

	// Draw the actual health bar based on the player's current health
	G_Rect(bx, by, bw * ratio, bh, hpCol);
}


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
		else
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(55, 95, 55, 255);
			armW = 16.0f;
			armL = r * 1.4f;
			armOffsetY = r * 0.2f;
		}

		G_Rect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);
		G_Rect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);

		CP_Settings_RectMode(CP_POSITION_CORNER);
		CP_Settings_NoFill();
		CP_Settings_Stroke(Col(20, 35, 20, 255));
		CP_Settings_StrokeWeight(1.5f);
		CP_Graphics_DrawRect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Graphics_DrawRect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Settings_NoStroke();

		G_Circle(x, y, r, skinColor);
		G_Ring(x, y, r, 2.0f, Col(20, 35, 20, 255));

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

static void Render(void)
{
	DrawBackgroundGrid();
	DrawZombies();
	DrawPlayerSprite();
	DrawMouseCrosshair();
	DrawHud();
}

/* =========================================================
   CProcessing Engine Callbacks
   ========================================================= */
void Game_Init(void)
{
	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	player.position = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.5f);
	player.aimAngleRadians = 0.0f;

	// Initialize player health
	player.hp = 100.0f;       // Player starts with 100 HP
	player.maxHp = 100.0f;    // Maximum player health is 100 HP
	player.ghostHp = 100.0f;  // Ghost health starts at maximum HP

	s_zombieColor = Col(95, 155, 85, 255);

	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
}

void Game_Update(void)
{
	float deltaTime = CP_System_GetDt();
	if (deltaTime > MAX_DELTA_TIME)
	{
		deltaTime = MAX_DELTA_TIME;
	}

	if (CP_Input_KeyTriggered(KEY_Q))
	{
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
		return;
	}
	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		CP_Engine_Terminate();
		return;
	}

	UpdatePlayerMovementAndAim(deltaTime);

	Render();
}

void Game_Exit(void)
{
}