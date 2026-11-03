//---------------------------------------------------------
// file:	game.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
//
// brief:   Source file containing the implementation of the 
//          game state, including initialization, game updates, and 
//          exit functions.
//
// Copyright © 2026 DigiPen, All rights reserved
//---------------------------------------------------------
//---------------------------------------------------------
#include "cprocessing.h" // Include the CProcessing library
#include "utils.h" // Include the utility functions 
#include "mainmenu.h" // Include the main menu state function
#include <math.h>
#include "game.h"

/* =========================================================
   TUNING CONSTANTS
   ========================================================= */
#define PLAYER_RADIUS        18.0f
#define PLAYER_SPEED         290.0f
#define GUN_BARREL_LENGTH    14.0f
#define GUN_BARREL_THICKNESS 9.0f
#define GRID_CELL_SIZE       100.0f
#define MAX_DELTA_TIME       0.05f
#define MAX_ZOMBIES 128



   /* =========================================================
	  PLAYER DATA STRUCTURE & GLOBAL STATE
	  ========================================================= */
typedef struct
{
	CP_Vector position;
	float     aimAngleRadians;
} Player;
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
static float  windowWidth = 1600.0f;
static float  windowHeight = 900.0f;
static Player player;

/* =========================================================
   HELPER FUNCTIONS
   ========================================================= */

   // Safely normalizes a vector so diagonal movement isn't faster than cardinal movement.
   // Returns (0, 0) if the player is standing still to avoid dividing by zero.
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
	float diameter = radius * 2.0f;

	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(color);
	CP_Graphics_DrawCircle(centerX, centerY, diameter);
}

static void DrawCircleOutline(float centerX, float centerY, float radius, float lineThickness, CP_Color color)
{
	float diameter = radius * 2.0f;

	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoFill();
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawCircle(centerX, centerY, diameter);
	CP_Settings_NoStroke();
}

static void DrawColoredLine(float startX, float startY, float endX, float endY, float lineThickness, CP_Color color)
{
	CP_Settings_Stroke(color);
	CP_Settings_StrokeWeight(lineThickness);
	CP_Graphics_DrawLine(startX, startY, endX, endY);
	CP_Settings_NoStroke();
}

/* =========================================================
   PLAYER MOVEMENT & AIMING LOGIC
   ========================================================= */
static void UpdatePlayerMovementAndAim(float deltaTime)
{
	// 1. Gather WASD directional input
	CP_Vector inputDirection = CP_Vector_Set(0.0f, 0.0f);

	if (CP_Input_KeyDown(KEY_W)) inputDirection.y -= 1.0f; // Up
	if (CP_Input_KeyDown(KEY_S)) inputDirection.y += 1.0f; // Down
	if (CP_Input_KeyDown(KEY_A)) inputDirection.x -= 1.0f; // Left
	if (CP_Input_KeyDown(KEY_D)) inputDirection.x += 1.0f; // Right

	// 2. Calculate new position based on speed and frame time
	CP_Vector moveDirection = GetNormalizedDirection(inputDirection);
	CP_Vector velocity = CP_Vector_Scale(moveDirection, PLAYER_SPEED * deltaTime);
	player.position = CP_Vector_Add(player.position, velocity);

	// 3. Keep the player inside the screen boundaries
	player.position.x = CP_Math_ClampFloat(player.position.x, PLAYER_RADIUS, windowWidth - PLAYER_RADIUS);
	player.position.y = CP_Math_ClampFloat(player.position.y, PLAYER_RADIUS, windowHeight - PLAYER_RADIUS);

	// 4. Calculate the angle from the player to the mouse cursor
	float mouseDeltaX = CP_Input_GetMouseX() - player.position.x;
	float mouseDeltaY = CP_Input_GetMouseY() - player.position.y;
	player.aimAngleRadians = atan2f(mouseDeltaY, mouseDeltaX);
}

/* =========================================================
   RENDERING FUNCTIONS
   ========================================================= */
static void DrawBackgroundGrid(void)
{
	CP_Color backgroundColor = CP_Color_Create(28, 34, 26, 255);
	CP_Color gridLineColor = CP_Color_Create(255, 255, 255, 10);

	CP_Graphics_ClearBackground(backgroundColor);

	// Draw vertical grid lines
	for (float x = 0.0f; x <= windowWidth; x += GRID_CELL_SIZE)
	{
		DrawColoredLine(x, 0.0f, x, windowHeight, 1.0f, gridLineColor);
	}

	// Draw horizontal grid lines
	for (float y = 0.0f; y <= windowHeight; y += GRID_CELL_SIZE)
	{
		DrawColoredLine(0.0f, y, windowWidth, y, 1.0f, gridLineColor);
	}
}

static void DrawPlayerSprite(void)
{
	CP_Color gunColor = CP_Color_Create(255, 230, 120, 255); // Yellow
	CP_Color bodyColor = CP_Color_Create(60, 130, 220, 255);  // Blue
	CP_Color highlightColor = CP_Color_Create(120, 185, 255, 255); // Light Blue

	// 1. Calculate where the tip of the gun barrel points using trigonometry
	CP_Vector aimDirection = CP_Vector_Set(cosf(player.aimAngleRadians), sinf(player.aimAngleRadians));
	float     barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	CP_Vector gunTipPos = CP_Vector_Add(player.position, CP_Vector_Scale(aimDirection, barrelReach));

	// 2. Draw gun barrel
	DrawColoredLine(
		player.position.x, player.position.y,
		gunTipPos.x, gunTipPos.y,
		GUN_BARREL_THICKNESS, gunColor
	);

	// 3. Draw circular player body
	DrawFilledCircle(player.position.x, player.position.y, PLAYER_RADIUS, bodyColor);

	// 4. Draw smaller off-center circle for a 3D shading highlight
	DrawFilledCircle(
		player.position.x - 4.0f,
		player.position.y - 4.0f,
		PLAYER_RADIUS * 0.45f,
		highlightColor
	);




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

static void DrawMouseCrosshair(void)
{
	float    mouseX = CP_Input_GetMouseX();
	float    mouseY = CP_Input_GetMouseY();
	CP_Color crosshairColor = CP_Color_Create(255, 230, 120, 230);
			G_Rect(x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));
			G_Rect(x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));
		}
	}
}

	// Outer ring
	DrawCircleOutline(mouseX, mouseY, 11.0f, 2.0f, crosshairColor);

	// Four directional ticks (Left, Right, Top, Bottom)
	DrawColoredLine(mouseX - 20.0f, mouseY, mouseX - 14.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX + 14.0f, mouseY, mouseX + 20.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY - 20.0f, mouseX, mouseY - 14.0f, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY + 14.0f, mouseX, mouseY + 20.0f, 2.0f, crosshairColor);
}

/* =========================================================
   CProcessing ENGINE CALLBACKS
   ========================================================= */
void Game_Init(void)
{
	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	// Spawn the player in the exact center of the window
	player.position = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.5f);
	player.aimAngleRadians = 0.0f;
	s_zombieColor = Col(95, 155, 85, 255);

	// Reset all zombie slots
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
}

void Game_Update(void)
{
	// Cap delta time so the player doesn't teleport if the window lags or is dragged
	float deltaTime = CP_System_GetDt();
	if (deltaTime > MAX_DELTA_TIME)
	{
		deltaTime = MAX_DELTA_TIME;
	}

	// Press ESC to close the game window
	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		CP_Engine_Terminate();
		return;
	}

	UpdatePlayerMovementAndAim(deltaTime);

	DrawBackgroundGrid();
	DrawPlayerSprite();
	DrawMouseCrosshair();
}

void Game_Exit(void)
{
	// Nothing to free yet
}