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
#include "cprocessing.h" // Include the CProcessing library
#include "utils.h" // Include the utility functions 
#include "game.h" // Include the game state functions


CP_Font myFont;

void Main_Menu_Init(void)
{
    // Load the font for the main menu
    myFont = CP_Font_Load("Assets/Exo2-Regular.ttf");
    //test
}

void Main_Menu_Update(void)
{
    // Clear the screen with a grey background
    CP_Graphics_ClearBackground(CP_Color_Create(100, 100, 100, 255));

    // Set the font for the main menu
    CP_Font_Set(myFont);

    // Set the rectangle color
    CP_Settings_Fill(CP_Color_Create(255, 150, 150, 255));
    CP_Settings_RectMode(CP_POSITION_CENTER);

    // draw two rectangles at the center of the screen
    CP_Graphics_DrawRect(800, 225, 300, 200);
    CP_Graphics_DrawRect(800, 625, 300, 200);

    // Set the text colour to black
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));

    // Set the text size
    CP_Settings_TextSize(60);

    // Set the text alignment to center both horizontally and vertically
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);

    // Draw the text for the buttons
    CP_Font_DrawText("Play", 800, 225);
    CP_Font_DrawText("Exit", 800, 625);

    // Check whether the player has clicked the mouse this frame
    if (CP_Input_MouseClicked())
    {
        // Store the current position of the mouse cursor
        float clickX = CP_Input_GetMouseX();
        float clickY = CP_Input_GetMouseY();
        // Check whether the mouse click is inside the Play button
        // IsAreaClicked() returns 1 if the click is inside the rectangle
        // and 0 if the click is outside the rectangle
        if (IsAreaClicked(800, 225, 300, 200, clickX, clickY))
        {
            // Change the game state to the game state
            CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
        }
        // Check whether the mouse click is inside the Exit button
        else if (IsAreaClicked(800, 625, 300, 200, clickX, clickY))
        {
            // Terminate the game
            CP_Engine_Terminate();
        }
    }

}

void Main_Menu_Exit(void)
{
    CP_Font_Free(myFont);
}