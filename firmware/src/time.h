/**
 * @file  time.h
 * @brief Definitions and prototypes for time functions
 *
 * @author Saint-Genest Gwenael <gwen@cowlab.fr>
 * @copyright Agilack (c) 2023
 *
 * @page License
 * Cowstick-UMS firmware is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 3 as published by the Free Software Foundation. You should have
 * received a copy of the GNU Lesser General Public License along with this
 * program, see LICENSE.md file for more details.
 * This program is distributed WITHOUT ANY WARRANTY.
 */
#ifndef TIME_H
#define TIME_H
#include "hardware.h"
#include "types.h"

#define SYSTICK_CTRL ((u32)(CM0_SYSTICK + 0x00))
#define SYSTICK_LOAD ((u32)(CM0_SYSTICK + 0x04))
#define SYSTICK_CUR  ((u32)(CM0_SYSTICK + 0x08))
#define SYSTICK_CAL  ((u32)(CM0_SYSTICK + 0x0C))

typedef struct tm_s
{
	u32 sec;
	u32 ms;
} tm_t;

void time_init(void);
u32  time_now(tm_t *timeval);
int  time_diff_ms(tm_t *ref);
int  time_since(u32 ref);

#endif
/* EOF */
