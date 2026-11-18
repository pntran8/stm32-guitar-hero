#include <stdio.h>
#include "main.h"
#include "stm32h5xx_it.h"
#include "Adafruit_ST7735_Macros.h"
#include "Adafruit_TFTShield18_Macros.h"
#include "dwt_module.h"
#include "Adafruit_TFTShield18_API.h"
#include "Adafruit_ST7735_API.h"
#include "stm32h5xx_hal.h"

#include "buzzer.h"
#include "stm32h5xx_hal_tim.h"
#include <stdbool.h>

// display:
// left port middle row -> 9th pin from the right in the top row of CN10
// right port middle row -> 2nd pin from the right in the top row of CN7

// BUTTONS: PC3, PC5, PC11, PB8
#define BUTTON_YELLOW_PIN GPIO_PIN_3 
#define BUTTON_YELLOW_PORT GPIOC

#define BUTTON_BLUE_PIN GPIO_PIN_5 
#define BUTTON_BLUE_PORT GPIOC

#define BUTTON_RED_PIN GPIO_PIN_11 
#define BUTTON_RED_PORT GPIOC

#define BUTTON_GREEN_PIN GPIO_PIN_8 
#define BUTTON_GREEN_PORT GPIOB

// BUZZER: PA8
#define BUZZER_PIN GPIO_PIN_8
#define BUZZER_PORT GPIOA

#define SCREEN_W 160
#define LANE_W 40
#define SCORE_BAR 14 // score bar at the top
#define NOTE_W 32
#define NOTE_H 10
#define NOTE_PAD 4 // (LANE_W - NOTE_W) / 2
#define TARGET_Y 112 // button target area at the bottom
#define TARGET_H 14
#define MAX_ON_SCREEN 6 // max notes visible at once
#define COLOR_GRAY 0xC618

// Define the states of the Simon game
typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    PLAYER_INPUT,
    GAME_WIN,
    GAME_LOSE
} State_t;

// static variable to keep track of the state
static State_t currentState = IDLE;

static uint8_t TEMP_COUNTODWN = 0;
static uint8_t TEMP_SCORE = 0;
static uint8_t TEMP_MISSES = 0;
static uint8_t TEMP_TOTNOTES = 0;
static uint8_t TEMP_BTN_PRESSED[4] = {false, false, false, false};
static uint8_t TEMP_ACTIVE_NOTES[MAX_ON_SCREEN] = {0};
static uint8_t TEMP_NOTE_LANES[MAX_ON_SCREEN] = {0};
static uint8_t TEMP_NOTE_Y[MAX_ON_SCREEN] = {0};


// Constants for GUI, tune when display is on
static const uint16_t brightColors[4] = {
    0xFFE0, // Yellow
    0x001F, // Blue
    0xF800, // Red
    0x07E0  // Green
};

static const uint16_t dimColors[4] = {
    0x5280, // Yellow
    0x000A, // Blue
    0x5000, // Red
    0x0200  // Green
};

static bool noteVisible(uint8_t i){
    return tmpNoteActive[i] &&
           tmpNoteY[i] >= SCORE_BAR &&
           tmpNoteY[i] <= (TARGET_Y - NOTE_H);
}

// Handles the graphics logic for the Simon Says game
void In_Game_Display(bool newScreen){ 
    bool previousSuccess = false;

    // Avoids updating the display if it does not need to be
    if(!updateDisplay){
        return;
    }
    updateDisplay = false; // Reset the flag after updating the display

    // Only fill the screen when a new screen is being shown to avoid overlapping
    if (currentState != previousState || levelSuccess != previousSuccess) {
        Display_fillScreen(ST7735_WHITE);
        previousState = currentState;
    }

    if (currentState == IDLE) {
        Display_setTextColor(ST7735_BLACK);
        Display_setTextSize(2, 2);
        Display_setCursor(14, 30);
        Display_print("GUITAR HERO");

        Display_setTextSize(1, 1);
        Display_setCursor(11, 76);
        Display_print("Press joystick to begin");

        // preview of the lanes
        for (uint8_t i = 0; i < 4; i++) {
            Display_fillRoundRect(24 + i * 30, 100, 22, 14, 3, brightColors[i]);
        }
        return;
    }
    if (currentState == COUNTDOWN) {
        for(uint8_t i = 3; i >= 1; i--){ // counts down numbers for a 3 second timer
            char seconds_string[2];
            sprintf(seconds_string, "%d", i); // converts current number to a string that Display_print can read
            // displays the number on screen every second
            Display_setTextSize(4, 4);
            Display_setCursor(68, 48);
            Display_setTextColor(ST7735_BLACK);
            Display_fillRect(60, 40, 40, 50, ST7735_WHITE);
            Display_print(seconds_string);
            //HAL_Delay(1000); // wait 1000 milliseconds, 1 second    
            if (waitMs(1000)) {
                return;
            }
        }
        return;
    }
    if (currentState == PLAY_SEQUENCE) {
        static int16_t lastY[MAX_ON_SCREEN];
        static uint8_t lastLane[MAX_ON_SCREEN];
        static bool lastPressed[4];
        static uint16_t lastScore, lastMisses;

        if (newScreen) {
            for (uint8_t i = 0; i < MAX_ON_SCREEN; i++) {
                lastY[i] = -1;
            }
            for (uint8_t i = 0; i < 4; i++) {
                lastPressed[i] = !tmpButtonPressed[i];
            } // force draw
            lastScore = lastMisses = 0xFFFF; // force draw

            // background: score bar and the lanes
            Display_fillRoundRect(0, SCORE_BAR - 1, SCREEN_W, 1, 0, COLOR_GRAY);
            for (uint8_t i = 1; i < 4; i++) {
                Display_fillRoundRect(i * LANE_W, SCORE_BAR, 1, TARGET_Y - SCORE_BAR, 0, COLOR_GRAY);
            }
        }
    }
    if (currentState == GAME_WIN) {
        Display_setTextSize(2, 2);
        Display_setCursor(32, 48);
        //Display_setTextColor(ST7735_GREEN);
        Display_setTextColor(tft.color565(31, 178, 56)); // darker green color
        Display_print("YOU WIN!");
    }
    if (currentState == GAME_LOSE) {
        Display_setTextSize(2, 2);
        Display_setCursor(26, 48);
        Display_setTextColor(ST7735_RED);
        Display_print("GAME OVER");
        return;
    }
}

void Guitar_Hero_State_Machine(void){
    switch(currentState){
        case IDLE:
        // welcome screen with the word “Guitar Hero” on the display and instructions to “press the joystick to begin” 
        // whenever the game is not in play
        // Pressing the “Select” button on the joystick should start the game.
        break;

        case COUNTDOWN:
        // The system should show a countdown from 3 before starting the game. The countdown must be measured by
        // a timer and NOT implemented using software delays.
        break;

        case PLAY_SEQUENCE:
        // While in play, the game should have notes sequentially fly down the screen.
            // a. The song must have at least 28 notes and 4 pitches.
            // b. The game must be checking for button presses while playing the song.
            // c. The game must keep track of how many correct/misses the user gets while playing.
        break;

        case PLAYER_INPUT:
        // see above comments
        break;

        case GAME_WIN:
        // Once the song is finished, there should be either a “you win” screen or a “you lose” screen depending 
        // on the user’s score. Both screens should also display the final score.
            // a. Both screens should display the final score.
            // b. Pressing the “Select” button on the joystick should bring the game back to the home screen.
        break;

        case GAME_LOSE:
        // see above comments
        break;

        default:
            currentState = IDLE;
        break;
    }
    return;
}