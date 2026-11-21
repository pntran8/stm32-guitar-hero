#include <stdio.h>
#include "main.h"
#include "stm32h533xx.h"
#include "stm32h5xx_it.h"
#include "graphics/Adafruit_ST7735_Macros.h"
#include "graphics/Adafruit_TFTShield18_Macros.h"
#include "dwt_module.h"
#include "graphics/Adafruit_TFTShield18_API.h"
#include "graphics/Adafruit_ST7735_API.h"
#include "stm32h5xx_hal.h"

//#include "buzzer.h"
#include "stm32h5xx_hal_tim.h"
#include <stdbool.h>

// defining GPIO fields
#define MODER_MASK(n) (GPIO_MODER_MODE0   << (2 * (n))) //2-bit mode field for pin n
#define MODER_OUT(n) (GPIO_MODER_MODE0_0 << (2 * (n))) //that field set to 01 = output
#define PUPDR_MASK(n) (GPIO_PUPDR_PUPD0   << (2 * (n))) //2-bit pull field for pin n
#define PUPDR_PU(n) (GPIO_PUPDR_PUPD0_0 << (2 * (n))) //that field set to 01 = pull-up
#define PIN_NUM(mask) (__builtin_ctz(mask)) //pin n's bit in ODR or IDR

// DEFINE BUTTON PORTS AND PINS
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

// Define the states of the Guitar Hero game
typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    GAME_WIN,
    GAME_LOSE
} State_t;

// DEFINE CONSTANTS FOR GAME ATTRIBUTES
#define NUM_BTNS 4
#define TIMER_1SEC 100
#define SONG_LENGTH 28 // TODO: replace with the actual value, this is a placeholder
#define WIN_SCORE 20 // TODO: replace with the actual value, this is a placeholder

// game variables for the current note of the song, and boolean flags for note start and
// checking if the note was correct
static uint8_t currentNote = 0;
static bool noteStarted = false;
static bool noteCorrect = false;
// score tracking variables
static uint8_t correct = 0;
static uint8_t missed = 0;

// Variables shared with TIM2 (must have volatile keyword)
static volatile State_t currentState = IDLE; // tracks current state
static volatile uint32_t stateTicks = 0; // keeps track of how many ticks have passed in this state
static volatile uint16_t noteTicks = 0; // keeps track of how many ticks the note needs to play
static volatile bool noteDone = false; // boolean flag for the note being completed
static volatile bool updateDisplay = true; // boolean flag to update the display

static volatile uint8_t buttons = 0; // keeps track of the buttons pressed
static volatile uint8_t newPresses = 0; // keeps track of new presses as an event

// Button GPIO arrays
static GPIO_TypeDef* BUTTON_PORTS[NUM_BTNS] = {BUTTON_YELLOW_PORT, BUTTON_BLUE_PORT, BUTTON_RED_PORT, BUTTON_GREEN_PORT}; //array of button ports
static const uint16_t BUTTON_PINS[NUM_BTNS] = {BUTTON_YELLOW_PIN, BUTTON_BLUE_PIN, BUTTON_RED_PIN, BUTTON_GREEN_PIN}; //array of button pins

// Initalizes all GPIO pins
void Game_GPIO_Init() {
    // wake up clocks first
    // RCC RCC_AHB2ENR
    // bit 0: IO port A
    // bit 1: IO port B
    // bit 2: IO port C

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR; //dummy read for clock to start before we try to use it

    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        // initialize btn pins to input w internal pull-up
        uint32_t btnN = PIN_NUM(BUTTON_PINS[i]);
        BUTTON_PORTS[i]->MODER &= ~MODER_MASK(btnN); // clear field -> 00 = input
        BUTTON_PORTS[i]->PUPDR &= ~PUPDR_MASK(btnN); // clear pull field
        BUTTON_PORTS[i]->PUPDR |= PUPDR_PU(btnN); // set 01 = pull-up  
    }
}

// Returns bit 0=yellow, bit 1=blue, bit 2=red, bit 3=green.
// Examples: yellow=0x01, red=0x04, yellow+red=0x05.
// reads 0x15 when all are pressed
uint8_t Game_ReadButtons(void)
{
    uint8_t pressed = 0;
    // loops through all buttons checking if the input register goes to 0 (0 = button press)
    // sets press equal to one bit shifted to ensure buttons return on different binary digits (eg. 0001, 0110, 0011)
    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        if ((BUTTON_PORTS[i]->IDR & BUTTON_PINS[i]) == 0) {
            pressed |= (uint8_t)(1 << i);
        }
    }
    return pressed;
}

// generates 100Hz clock for game timing
// adapted frome: https://blog.embeddedexpert.io/working-with-stm32-and-timers-part-3-timer-interrupt/ and fit to our program
void Game_Timer_Init(void)
{
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN; // enable clock
    TIM2->CR1 = 0; // reset value
    TIM2->DIER = 0; // reset value
    TIM2->PSC = 31999; // 32 MHz / 32000 = 1 kHz
    TIM2->ARR = 9; // 1 kHz / 10 = 100 Hz
    TIM2->EGR = TIM_EGR_UG; // force load prescaler
    TIM2->SR = ~TIM_SR_UIF; // clear update from UG
    TIM2->CNT = 0; // resets counter to 0
    NVIC_ClearPendingIRQ(TIM2_IRQn); // clear any interupt status left in CPU
    TIM2->DIER = TIM_DIER_UIE; // enables interupts
    NVIC_EnableIRQ(TIM2_IRQn); // enables specifically TIM2 as an interupt
    TIM2->CR1 = TIM_CR1_CEN; // starts counter timer
}

// Main interupt function, called in the TIM2 ISR
void Game_TimerTick(void)
{
    // debounces button keeping track of how long the input stays the same
    static uint8_t lastRaw = 0;
    static uint8_t sameCount = 0;
    uint8_t raw = Game_ReadButtons();
    // Debounce buttons
    if (raw == lastRaw) {
        if (sameCount < 2)
            sameCount++;
    } else {
        sameCount = 0;
    }
    // remembers which button for comparison later
    lastRaw = raw;
    // if it has been stable long enough and is different from the last button state
    if (sameCount == 2 && raw != buttons) {
        // set new presses equal to buttons that move from not pressed (0) to pressed (1)
        newPresses |= raw & (uint8_t)~buttons;
        buttons = raw;
    }
    // measures elapsed time in 10ms units
    stateTicks++;

    // timing for countdown state
    // if the current state is countdown start timing
    if (currentState == COUNTDOWN) {
        // every 1 second (100 ticks %100Hz) 
        if (stateTicks % TIMER_1SEC == 0)
            updateDisplay = true;
        // once it has gotten to 3 seconds (300 ticks) end the countdown
        if (stateTicks >= 3 * TIMER_1SEC) {
            // enter next state, update display flag true, resets how many ticks has passed
            currentState = PLAY_SEQUENCE;
            stateTicks = 0;
            updateDisplay = true;
        }
    }
    // check if the game is playing a song, the current note has a duration, and it is till being played
    else if (currentState == PLAY_SEQUENCE &&
             noteTicks != 0 &&
             stateTicks >= noteTicks) {
        // flag that the note is done and reset the counter
        noteDone = true;
        noteTicks = 0;
    }
}


// Main-loop helper: fetch new presses once while preserving IRQ state.
uint8_t Game_TakeButtonPresses(void)
{
    __disable_irq();
    uint8_t events = newPresses;
    newPresses = 0;
    __enable_irq();

    return events;
}


void Game_StartNote(uint16_t duration)
{
    // save IRQ state and disable for variable checking
    uint32_t irqState = __get_PRIMASK();
    __disable_irq();
    // check relavant variables
    stateTicks = 0;
    noteTicks = duration;
    noteDone = false;
    // set IRQ state back to original state
    __set_PRIMASK(irqState);
}

void Guitar_Hero_State_Machine(void){
    // tracks if select ever gets pressed during the run of the state machine
    bool selectPressed = !(TFTShield18_readButtons() & TFTSHIELD_BUTTON_IN);
    // switch state for each possible guitar hero state
    switch(currentState){
        case IDLE:
        // TODO: Welcome screen with the word “Guitar Hero” on the display and instructions to “press the joystick to begin” 
        // resets everything and starts the game when select is pressed in the IDLE state
        if (selectPressed) {
            currentNote = 0;
            correct = 0;
            missed = 0;
            noteStarted = false;
            noteCorrect = false;
            // save IRQ state and disable to check variables
            uint32_t irqState = __get_PRIMASK();
            __disable_irq();
            // check relevant variables
            stateTicks = 0;
            noteTicks = 0;
            noteDone = false;
            newPresses = 0;
            // update relevant variables
            currentState = COUNTDOWN;
            updateDisplay = true;
            // set IRQ state back to the original state
            __set_PRIMASK(irqState);
        }
        break;

        case COUNTDOWN:
        // The system should show a countdown from 3 before starting the game. T
        // when update display is true, reset it and then display 3-2-1 depending on the amount of ticks that has passed
        if (updateDisplay) {
            updateDisplay = false;
            if (stateTicks < 100) {
                // Display 3
            }
            else if (stateTicks < 200) {
                // Display 2
            }
            else {
                // Display 1
            }
        }
        break;

        case PLAY_SEQUENCE:
        // TODO: While in play, the game should have notes sequentially fly down the screen.
        // TODO: The song must have at least 28 notes and 4 pitches.
        // if select is pressed reset everything and return to IDLE
        if (selectPressed) {
            __disable_irq();
            currentState = IDLE;
            noteTicks = 0;
            noteDone = false;
            newPresses = 0;
            updateDisplay = true;
            __enable_irq();

            noteStarted = false;
            // TODO: Stop buzzer
            break;
        }

        // Start the current note
        if (!noteStarted) {
            noteCorrect = false;
            Game_StartNote(/*DURATION OF NOTE IN SONG*/);
            noteStarted = true;

            // TODO: Draw note
            // TODO: Play song[currentNote].frequency
        }

        // Read the button events detected by TIM2, check button presses throughout
        uint8_t pressed = Game_TakeButtonPresses();

        // check if the button the user pressed is correct
        if (!noteCorrect && pressed == /*BUTTON ASSOCIATE WITH NOTE IN SONG*/) {
            correct++;
            noteCorrect = true;
        }

        // retrieve the note completion event
        bool finished;
        // disable IRQ and save state
        uint32_t irqState = __get_PRIMASK();
        // check if the note is finished playing
        __disable_irq();
        finished = noteDone;
        noteDone = false;
        // set IRQ state back to the original state
        __set_PRIMASK(irqState);
        // if it is finished, check if the note was not correct, and advance the song
        if (finished) {
            noteStarted = false;
            if (!noteCorrect)
                missed++;
            // TODO: Stop buzzer
            currentNote++;
            // check if the song is over and if the score is high enough to be winning
            if (currentNote >= SONG_LENGTH) {
                currentState = (correct >= WIN_SCORE) ? GAME_WIN : GAME_LOSE;
                updateDisplay = true;
            }
        }
        break;

        case GAME_WIN:
        // TODO: Screen should display a winning message
        // TODO: Screens should display the final score.
        // if select is pressed reset everything and return to IDLE
        if (selectPressed) {
            currentState = IDLE;
            updateDisplay = true;
        }
        break;

        case GAME_LOSE:
        // TODO: Screen should display a losing message
        // TODO: Screens should display the final score.
        // if select is pressed reset everything and return to IDLE
        if (selectPressed) {
            currentState = IDLE;
            updateDisplay = true;
        }
        break;

        // defualt catchall to set unforseen behavior to IDLE
        default:
            currentState = IDLE;
        break;
    }
    return;
}