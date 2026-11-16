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

void Guitar_Hero(void){
    return;
}