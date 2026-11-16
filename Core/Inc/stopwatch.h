/**
 * stopwatch.h — ECE2049 Lecture 3.1 stop watch example (TIM2, bare-register)
 *
 * TIM2 generates an update interrupt every 1/100 second. The ISR counts
 * interrupts in the global variable `timer`, so `timer` holds elapsed time in
 * hundredths of a second.
 */
#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <stdint.h>

/* Global timer count variable (hundredths of a second). volatile because the
 * ISR changes it while main() is reading it. */
extern volatile uint32_t timer;

void runTimer2(void);              // configure TIM2 for 100 Hz and start it
void stopTimer2(void);             // stop TIM2 and reset the count to 0
void displayTime(uint32_t t);      // show t (hundredths of a second) on the display

#endif /* STOPWATCH_H */
