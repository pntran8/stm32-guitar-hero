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

// PORT: GPIOA, GPIOB, GPIOC
// PIN: PIN NUMBER (0-15)
// LEDS: PC12, PC 10, PC0, PC2
#define LED_YELLOW_PIN GPIO_PIN_12
#define LED_YELLOW_PORT GPIOC

#define LED_BLUE_PIN GPIO_PIN_10 
#define LED_BLUE_PORT GPIOC

#define LED_RED_PIN GPIO_PIN_0 
#define LED_RED_PORT GPIOC

#define LED_GREEN_PIN GPIO_PIN_2 
#define LED_GREEN_PORT GPIOC

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

// Define the states of the Simon game
typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    PLAYER_INPUT,
    GAME_WIN,
    GAME_LOSE
} State_t;

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

// Positions for each button rectangle
static const int16_t xPos[4] = {20, 86, 20, 86};
static const int16_t yPos[4] = {22, 22, 66, 66};

// Handles the graphics logic for the Simon Says game
void In_Game_Display(void){ 
    static State_t previousState = GAME_LOSE;
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
    // Level success screen
    if (levelSuccess) {
        Display_setTextSize(2, 2);
        Display_setCursor(32, 48);
        Display_setTextColor(tft.color565(31, 178, 56)); // darker green color
        Display_print("GOOD JOB!");
        return;
    }
    
    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_BLACK);
    // Handles screens for each state except for coutdown
    if (currentState == IDLE) {
        Display_setCursor(24, 35);
        Display_print("Simon Says Game");
        Display_setTextSize(1, 1);
        Display_setCursor(24, 76);
        Display_print("Press SELECT to start");
        return;
    }
    if (currentState == GAME_WIN) {
        Display_setTextSize(2, 2);
        Display_setCursor(32, 48);
        //Display_setTextColor(ST7735_GREEN);
        Display_setTextColor(tft.color565(31, 178, 56)); // darker green color
        Display_print("YOU WIN!");
        return;
    }
    if (currentState == GAME_LOSE) {
        Display_setTextSize(2, 2);
        Display_setCursor(26, 48);
        Display_setTextColor(ST7735_RED);
        Display_print("GAME OVER");
        return;
    }

    // Displays the level at the top of the screen
    char levelString[32];
    snprintf(levelString, sizeof(levelString), "Level: %u / %u", CURRENT_LEVEL, MAX_SEQ);
    Display_setTextColor(ST7735_BLACK);
    Display_setTextSize(1, 1);
    Display_setCursor(8, 5);
    Display_print(levelString);

    // Loops through drawing each button (avoids redundant code)
    for (uint8_t i = 0; i < 4; i++) {
        Display_fillRoundRect(xPos[i], yPos[i], 54, 39, 6, 
            buttonPressed[i] ? brightColors[i] : dimColors[i]);
    }
    
    // Displays the last five buttons pressed at the bottom of the screen
    Display_setTextColor(ST7735_BLACK);
    Display_setCursor(8, 115);
    Display_print("Last 5:");
    // loops through drawing each button color to avoid redundant code
    for (uint8_t i = 0; i < HISTORY_COUNTER; i++) {
        Display_fillCircle( 66 + i * 17, 118, 5,brightColors[BUTTON_HISTORY[i]]);
    }
}

void Guitar_Hero(void){
    return;
}