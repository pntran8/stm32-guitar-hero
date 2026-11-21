#include <stdint.h>
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


// ================================= display stuff =========================================
#define SCREEN_W 160
#define LANE_W 40
#define SCORE_BAR 14 // score bar at the top
#define NOTE_W 32
#define NOTE_H 10
#define NOTE_PAD 4 // (LANE_W - NOTE_W) / 2
#define TARGET_Y 112 // button target area at the bottom
#define TARGET_H 14
#define COLOR_GRAY 0xC618
#define NOTE_TRAVEL (TARGET_Y - NOTE_H - SCORE_BAR) // 88 px of fall
#define COLOR_NOTE_HIT 0x7BEF
#define HIT_WINDOW_PX 16 // note's bottom edge within this many px of the target

static const uint16_t brightColors[4] = {0xFFE0, 0x001F, 0xF800, 0x07E0}; // yellow, blue, red, green
static const uint16_t dimColors[4] = {0x5280, 0x000A, 0x5000, 0x0200};

// TEMP song stuff
// REPLACE ONCE THE SONG TABLE EXISTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  
// lane = button index (0, 1, 2, 3) (yellow, blue, red, green) = bit i of Game_ReadButtons()
// if song stores button as a mask, use PIN_NUM(mask)
static inline uint8_t  Disp_NoteLane(uint8_t index) { 
    return index % 4; 
}      // TODO: song[index].button
static inline uint16_t Disp_NoteDuration(uint8_t index) { 
    (void)index; 
    return 50; //right now each note only lasts 50 ticks
} // TODO: song[index].duration (10 ms ticks)

// idle state display
static void draw_idle(void){
    Display_setTextColor(ST7735_BLACK);
    Display_setTextSize(1, 1);
    Display_setCursor(50, 16);
    Display_print("WELCOME TO");

    Display_setTextSize(2, 2);
    Display_setCursor(14, 30);
    Display_print("GUITAR HERO");

    Display_setTextSize(1, 1);
    Display_setCursor(17, 76);
    Display_print("Press SELECT to begin");

    for (uint8_t i = 0; i < 4; i++) {
        Display_fillRoundRect(24 + i * 30, 100, 22, 14, 3, brightColors[i]);
    }}

// game win or lose display
static void draw_ending(bool win){
    char buf[24];

    Display_setTextSize(2, 2);
    if (win) {
        Display_setCursor(26, 24);
        Display_setTextColor(0x1D87);
        Display_print("YOU WIN!!");      // 108 px
    } else {
        Display_setCursor(14, 24);
        Display_setTextColor(ST7735_RED);
        Display_print("YOU LOSE :(");    // 132 px
    }

    Display_setTextColor(ST7735_BLACK);
    snprintf(buf, sizeof(buf), "Score: %u/%u", (unsigned)correct, (unsigned)SONG_LENGTH);
    Display_setCursor(8, 60);
    Display_print(buf);

    Display_setTextSize(1, 1);
    Display_setCursor(14, 96);
    Display_print("Press SELECT to return");
    Display_setCursor(38, 108);
    Display_print("to home screen");
}

//countdown display
static void draw_countdown(bool newScreen){
    static uint8_t lastShown = 0xFF;

    uint32_t secs = stateTicks / TIMER_1SEC; // 0, 1, 2
    uint8_t number = (secs >= 3) ? 1 : (uint8_t)(3 - secs); // 3, 2, 1

    if (!newScreen && number == lastShown) { //not new screen and still the same number
        return; // nothing changed
    }
    lastShown = number;

    if (!newScreen) {
        Display_fillScreen(ST7735_WHITE); // dispatcher already cleared if new
    }

    Display_setTextColor(ST7735_BLACK);
    Display_setTextSize(1, 1);
    Display_setCursor(53, 12);
    Display_print("GET READY");

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", (unsigned)number); //show number
    Display_setTextSize(6, 6);
    Display_setCursor(62, 40);
    Display_print(buf);
}

// play sequence display
static void draw_play(bool newScreen){
    static int16_t lastY = -1;
    static uint8_t lastLane = 0;
    static bool lastHit = false;
    static uint8_t lastCorrect, lastMissed, lastButtons;

    // snapshot shared vars once per frame
    bool started = noteStarted;
    uint8_t index = currentNote;
    uint32_t ticks = stateTicks;
    uint8_t btns = buttons;
    bool hit = noteCorrect;

    if (newScreen) {
        lastY = -1;
        lastCorrect = lastMissed = 0xFF; // force HUD draw
        lastButtons = (uint8_t)~btns; // force all four targets to draw

        Display_fillRoundRect(0, SCORE_BAR - 1, SCREEN_W, 1, 0, COLOR_GRAY);
        for (uint8_t i = 1; i < 4; i++) {
            Display_fillRoundRect(i * LANE_W, SCORE_BAR, 1, TARGET_Y - SCORE_BAR, 0, COLOR_GRAY);
        }
    }

    // score bar
    if (correct != lastCorrect || missed != lastMissed) {
        lastCorrect = correct;
        lastMissed  = missed;

        char buf[16];
        Display_fillRoundRect(0, 0, SCREEN_W, SCORE_BAR - 1, 0, ST7735_WHITE);
        Display_setTextColor(ST7735_BLACK);
        Display_setTextSize(1, 1);

        snprintf(buf, sizeof(buf), "Hits: %u", (unsigned)correct);
        Display_setCursor(4, 3);
        Display_print(buf);

        snprintf(buf, sizeof(buf), "Miss: %u", (unsigned)missed);
        Display_setCursor(90, 3);
        Display_print(buf);
    }

    // falling note
    int16_t newY = -1; // -1 is no note on screen
    uint8_t lane = lastLane;
    if (started) {
        uint16_t dur = Disp_NoteDuration(index);

        if (dur == 0) {
            dur = 1;
        }

        uint32_t t = (ticks > dur) ? dur : ticks;
        newY = SCORE_BAR + (int16_t)((uint32_t)NOTE_TRAVEL * t / dur);
        lane = Disp_NoteLane(index);
    }

    bool changed = (newY != lastY) || (newY >= 0 && (lane != lastLane || hit != lastHit));
    if (changed) {
        bool slidDown = (newY >= 0 && lastY >= 0 && lane == lastLane && newY >= lastY);

        if (!slidDown && lastY >= 0) { // note ended / new note / lane changed: wipe old one
            Display_fillRoundRect(lastLane * LANE_W + NOTE_PAD, lastY, NOTE_W, NOTE_H, 0, ST7735_WHITE);
        }

        if (newY >= 0) {
            int16_t x = lane * LANE_W + NOTE_PAD;
            Display_fillRoundRect(x, newY, NOTE_W, NOTE_H, 3, hit ? COLOR_NOTE_HIT : brightColors[lane]);
            
            if (slidDown && newY > lastY) { // erase only the strip the note just left
                int16_t h = newY - lastY;

                if (h > NOTE_H) {
                    h = NOTE_H;
                }

                Display_fillRoundRect(x, lastY, NOTE_W, h, 0, ST7735_WHITE);
            }
        }

        lastY = newY;
        lastLane = lane;
        lastHit = hit;
    }

    // bottom targets light up when button is held
    uint8_t diff = btns ^ lastButtons;
    for (uint8_t i = 0; i < 4; i++) {
        if (!(diff & (1u << i))) {
            continue;
        }
        Display_fillRoundRect(i * LANE_W + NOTE_PAD, TARGET_Y, NOTE_W, TARGET_H, 4, (btns & (1u << i)) ? brightColors[i] : dimColors[i]);
    }
    lastButtons = btns;
}

void Guitar_Hero_Display(void){
    static bool firstRun = true;
    static State_t previousState = IDLE;

    bool force = updateDisplay; // state machine / ISR can still request a full redraw
    updateDisplay = false; // clear BEFORE drawing so a request during drawing isn't lost

    State_t state = currentState; // snapshot of the volatile
    bool newScreen = firstRun || force || (state != previousState);
    firstRun = false;
    previousState = state;

    if (newScreen) {
        Display_fillScreen(ST7735_WHITE);
    }

    switch (state) {
        case IDLE:
            if (newScreen) {
                draw_idle();
            }
            break;
        case COUNTDOWN:
            draw_countdown(newScreen);
            break;
        case PLAY_SEQUENCE: 
            draw_play(newScreen);
            break;
        case GAME_WIN:
            if (newScreen) {
                draw_ending(true);
            }
            break;
        case GAME_LOSE:
            if (newScreen) {
                draw_ending(false);
            } 
            break;
        default: 
            break;
    }
}

// ================================= display stuff ends =========================================


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
            Game_StartNote(Disp_NoteDuration(currentNote));
            noteStarted = true;

            // TODO: Draw note
            // TODO: Play song[currentNote].frequency
        }

        // Read the button events detected by TIM2, check button presses throughout
        uint8_t pressed = Game_TakeButtonPresses();

        // check if the button the user pressed is correct
        uint16_t duration = Disp_NoteDuration(currentNote);
        uint32_t windowTicks = ((uint32_t)HIT_WINDOW_PX * duration) / NOTE_TRAVEL;
        if (windowTicks < 1) {
            windowTicks = 1;
        }
        bool inWindow = (stateTicks + windowTicks >= duration);       
        if (!noteCorrect && inWindow && pressed == (1u << Disp_NoteLane(currentNote))) {
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