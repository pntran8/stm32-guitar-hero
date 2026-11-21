#ifndef GUITAR_HERO_H
#define GUITAR_HERO_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "stm32h5xx_it.h"

void Game_GPIO_Init(void);
void Game_Timer_Init(void);
void Game_TimerTick(void);
void Guitar_Hero_State_Machine(void);
void Guitar_Hero_Display(void);

#endif