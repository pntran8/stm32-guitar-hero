/**
 * stopwatch.c — ECE2049 Lecture 3.1 stop watch example (TIM2, bare-register)
 *
 * Timer clock: 32 MHz (default clocks: SYSCLK = HCLK = PCLK1 = 32 MHz, so TIM2
 * on APB1 is clocked at 32 MHz).
 *
 *   update rate = 32 MHz / ((PSC + 1) * (ARR + 1))
 *               = 32 MHz / (32 * 10000) = 100 Hz  ->  one interrupt per 0.01 s
 */
#include <stdio.h>

#include "stm32h5xx.h"
#include "stopwatch.h"
#include "Adafruit_ST7735_API.h"
#include "Adafruit_ST7735_Macros.h"

volatile uint32_t timer = 0;   // hundredths of a second since start

void runTimer2(void)
{
    // enable TIM2's clock (|= : other APB1 peripherals are already on)
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;

    TIM2->PSC  = 31;              // 32 MHz / (31+1) = 1 MHz -> 1 us per tick
    TIM2->ARR  = 9999;            // 9999+1 = 10000 ticks = 1/100 seconds
    TIM2->EGR  = TIM_EGR_UG;      // load PSC now (also sets UIF:
                                  // the first interrupt arrives right away)
    TIM2->DIER = TIM_DIER_UIE;    // update interrupt enabled
    NVIC_EnableIRQ(TIM2_IRQn);    // TIM2's interrupt line enabled in the NVIC
    TIM2->CR1  = TIM_CR1_CEN;     // start counting
}

void stopTimer2(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;    // stop counting
    NVIC_DisableIRQ(TIM2_IRQn);
    timer = 0;                    // reset the stop watch
}

// The name must match the vector table in startup_stm32h533xx.s exactly.
void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;       // clear the update flag: write 0, don't modify

    timer++;                      // one more 1/100 second has passed

    // blink the green LED (PA5) once per second
    if (timer % 100 == 0)
        GPIOA->BSRR = GPIO_BSRR_BS5;   // LED on
    else if (timer % 100 == 50)
        GPIOA->BSRR = GPIO_BSRR_BR5;   // LED off
}

void displayTime(uint32_t t)
{
    char str[24];
    uint32_t seconds    = t / 100;
    uint32_t hundredths = t % 100;

    snprintf(str, sizeof(str), "%5lu.%02lu s",
             (unsigned long)seconds, (unsigned long)hundredths);

    Display_setTextSize(2, 2);
    Display_setTextColorWithBg(ST7735_BLACK, ST7735_WHITE);  // overwrite old digits
    Display_setCursor(20, 56);
    Display_print(str);
}
