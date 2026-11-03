//---------------------------------------------------------
// file:    mainmenu.c
// author:  LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:   Main menu state for the game, where the player 
//          can choose to start the game or exit.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"
#include "game.h"
#include "mainmenu.h"

static CP_Font myFont;

void Main_Menu_Init(void)
{
    // Load the font for the main menu using standard Assets path
    myFont = CP_Font_Load("Assets/Exo2-Regular.ttf");
}

void Main_Menu_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    // Button dimensions
    float btn_w = 300.0f;
    float btn_h = 200.0f;
    float center_x = w * 0.5f;
    float play_y = h * 0.25f;
    float exit_y = h * 0.70f;

    // Clear background
    CP_Graphics_ClearBackground(CP_Color_Create(100, 100, 100, 255));

    // Set font
    CP_Font_Set(myFont);

    // Draw button rectangles
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(255, 150, 150, 255));
    CP_Graphics_DrawRect(center_x, play_y, btn_w, btn_h);
    CP_Graphics_DrawRect(center_x, exit_y, btn_w, btn_h);

    // Draw text inside buttons
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_TextSize(60.0f);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Font_DrawText("Play", center_x, play_y);
    CP_Font_DrawText("Exit", center_x, exit_y);

    // Handle mouse click
    if (CP_Input_MouseClicked())
    {
        float clickX = CP_Input_GetMouseX();
        float clickY = CP_Input_GetMouseY();

        if (IsAreaClicked(center_x, play_y, btn_w, btn_h, clickX, clickY))
        {
            CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
        }
        else if (IsAreaClicked(center_x, exit_y, btn_w, btn_h, clickX, clickY))
        {
            CP_Engine_Terminate();
        }
    }
}

void Main_Menu_Exit(void)
{
    CP_Font_Free(myFont);
}