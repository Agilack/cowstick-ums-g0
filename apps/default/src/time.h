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
#include "types.h"

typedef struct tm_s
{
	u32 sec;
	u32 ms;
} tm_t;

extern u32  (*time_now)(tm_t *timeval);
//int  time_diff_ms(tm_t *ref);
extern int  (*time_since)(u32 ref);

#endif
/* EOF */
