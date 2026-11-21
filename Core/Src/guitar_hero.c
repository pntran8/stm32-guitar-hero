#include <stdio.h>
#include "main.h"
#include "stm32h5xx_it.h"
#include "Adafruit_ST7735_Macros.h"
#include "Adafruit_TFTShield18_Macros.h"
#include "dwt_module.h"
#include "Adafruit_TFTShield18_API.h"
#include "Adafruit_ST7735_API.h"
#include "stm32h5xx_hal.h"

#include "stopwatch.h"
#include "buzzer.h"
#include "stm32h5xx_hal_tim.h"
#include <stdbool.h>

// display:
// left port middle row -> 9th pin from the right in the top row of CN10
// right port middle row -> 2nd pin from the right in the top row of CN7

//Twinke, Twinkle, Little Star
//C C G G A A G    ← hold G for 2 beats
//F F E E D D C    ← hold C for 2 beats
//G G F F E E D    ← hold D for 2 beats
//G G F F E E D    ← hold D for 2 beats
//C C G G A A G    ← hold G for 2 beats
//F F E E D D C    ← hold C for 2 beats

// C -> BUTTON_RED
// D, E -> BUTTON_YELLOW
// F, G -> BUTTON_GREEN
// A -> BUTTON_BLUE


//wrouegbhwrpighjeaortbgjoanernGIA;GNR

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

// song definitions
#define TICKS_PER_BEAT 50
#define TICKS_GAP 5 //reserve time after each note for silence
#define SONG_LENGTH (sizeof melody / sizeof melody[0]) // the length of the song by entries, this is 42

extern Buzzer buzzer1; // found buzzer1 in main

typedef struct { 
    Note pitch; // the Notes in buzzer.h. eg. NOTE_C, NOTE_F
    uint8_t beat; // how many beats the note occupies
    uint8_t buttonMask; // expected button for this note
} SongNote; // each individual note in a song

enum { SONG_YELLOW = 0x01, SONG_BLUE = 0x02, SONG_RED = 0x04, SONG_GREEN = 0x08 };

static const SongNote melody[] = {
    // Twinkle, twinkle, little star
    {NOTE_C, 1, SONG_RED},
    {NOTE_C, 1, SONG_RED},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_A * 2, 1, SONG_BLUE},
    {NOTE_A * 2, 1, SONG_BLUE},
    {NOTE_G, 2, SONG_GREEN},

    // How I wonder what you are
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_D, 1, SONG_YELLOW},
    {NOTE_D, 1, SONG_YELLOW},
    {NOTE_C, 2, SONG_RED},

    // Up above the world so high
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_D, 2, SONG_YELLOW},

    // Like a diamond in the sky
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_D, 2, SONG_YELLOW},

    // Twinkle, twinkle, little star
    {NOTE_C, 1, SONG_RED},
    {NOTE_C, 1, SONG_RED},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_G, 1, SONG_GREEN},
    {NOTE_A * 2, 1, SONG_BLUE},
    {NOTE_A * 2, 1, SONG_BLUE},
    {NOTE_G, 2, SONG_GREEN},

    // How I wonder what you are
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_F, 1, SONG_GREEN},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_E, 1, SONG_YELLOW},
    {NOTE_D, 1, SONG_YELLOW},
    {NOTE_D, 1, SONG_YELLOW},
    {NOTE_C, 2, SONG_RED},
};

// This starts the tone. The game loop advances currentNote when TIM2 signals completion.
static void playCurrentNote(uint8_t currentNote){
    if (currentNote >= SONG_LENGTH) {
        return;
    }
    Note pitch = melody[currentNote].pitch;

    buzzerSetNote(&buzzer1, pitch, 1);
    buzzerEnable(&buzzer1);
}

// Isaac: this is the version of Disp_NoteDuration to use to get the not to run the right amount of ticks
static inline uint16_t Disp_NoteDuration(uint8_t index)
{
    // With 50 ticks per beat, a one-beat note gets 50 and a two-beat note gets 100.
    return melody[index].beat * TICKS_PER_BEAT;
}

// Isaac: this is supposed to get the notes in the correct lane
static inline uint8_t Disp_NoteLane(uint8_t index)
{
    // Convert the song's button mask to the screen's lane number.
    // Example: red mask 0x04 becomes lane 2.
    return (uint8_t)PIN_NUM(melody[index].buttonMask);
}

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
            if (!noteStarted) {
                noteCorrect = false;
                Game_StartNote(Disp_NoteDuration(currentNote));
                playCurrentNote(currentNote);
                noteStarted = true;
            }

            uint16_t duration = Disp_NoteDuration(currentNote);  // the intention with this is to make each note sound distinct by putting a little gap inbetween them
            if (buzzer1.currBuzzerState && stateTicks >= duration - TICKS_GAP) {
                buzzerDisable(&buzzer1);
            }
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