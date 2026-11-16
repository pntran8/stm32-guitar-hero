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