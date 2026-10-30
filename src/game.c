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
#include "cprocessing.h" // Include the CProcessing library
#include "utils.h" // Include the utility functions 
#include "mainmenu.h" // Include the main menu state function
#include <stdbool.h> //include boolean

//enums
typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType; //for different type of zombies


//zombie structure, zombie stats 
typedef struct
{
	bool active; //check if zombie is active
	int id;
	ZombieType type;
	CP_Vector pos; //coordinates / position of the zombie
	float hp, maxHp, speed, radius, damage;
	float attackTimer, swayPhase, swayAmp, hitFlash; 
} Zombie;

static void DrawZombies(void) {
	CP_Color defaultColor = Col(95, 155, 85, 255); // Default color for zombies
}

void Game_Init(void)
{
	
}


void Game_Update(void)
{
	
}

void Game_Exit(void)
{

}