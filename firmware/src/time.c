/**
 * @file  time.c
 * @brief The time module is used to manage time and duration
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
#include "time.h"
#include "uart.h"

u32 ticks;
u32 time_ms;
u32 time_s;

/**
 * @brief Initialize time module
 *
 * This function configure and start the Systick timer to have an interrupt
 * every 1ms. This interrrupt is the time reference for all other functions.
 * To work properly, this function must be called on startup (at least before
 * calling any other time function).
 */
void time_init(void)
{
	ticks   = 0;
	time_ms = 0;
	time_s  = 0;

	/* Configure Systick */
	reg_wr(SYSTICK_LOAD,  64000);
	reg_wr(SYSTICK_CTRL, (1 << 2) | (1 << 1) | 1);
}

/**
 * @brief Get the current time reference in ticks or in seconds
 *
 * This function is used to get the current time (elapsed since boot). If the
 * timeval argument is non-null the pointed structure is filled with number of
 * seconds and milliseconds (optional). This function always returns the value
 * of ticks counter (incremented every 1ms).
 *
 * @param timeval Pointer to a time structure (optional)
 * @return integer Current value of ticks counter
 */
u32 time_now(tm_t *timeval)
{
	if (timeval)
	{
		timeval->sec = time_s;
		timeval->ms  = time_ms;
	}

	return(ticks);
}

/**
 * @brief Compute the difference between now and a reference timestamp
 *
 * @param ref Pointer to a time structure with reference timestamp
 * @return integer Number of ms between time ref and now
 */
int time_diff_ms(tm_t *ref)
{
	u32 now_s, now_ms;
	int result;

	/* Get a copy of current time */
	now_s  = (volatile u32)time_s;
	now_ms = (volatile u32)time_ms;
	if (now_s != (volatile u32)time_s)
	{
		now_s  = (volatile u32)time_s;
		now_ms = (volatile u32)time_ms;
	}

	/* If reference and now are into the same second */
	if (ref->sec == now_s)
	{
		result = (int)(now_ms - ref->ms);
	}
	/* If reference is into the past */
	else if (ref->sec < now_s)
	{
		/* This may overflow if cowstick is used for 60yr :p */
		result  = (int)(1000 - ref->ms);
		result += (int)((now_s - ref->sec - 1) * 1000);
		result += (int)now_ms;
	}
	/* If reference is into the future */
	else
	{
		result  = (int)(0 - (1000 - now_ms));
		result -= (int)((ref->sec - now_s - 1) * 1000);
		result -= (int)ref->ms;
	}

	return(result);
}

/**
 * @brief Compute the difference between a ticks ref and current time
 *
 * @param ref Value to compare with current ticks counter
 * @return integer Difference (in ticks)
 */
int time_since(u32 ref)
{
	int tm_diff;

	tm_diff = (int)(ticks - ref);

	return(tm_diff);
}

/**
 * @brief Systick interrupt handler
 */
void SysTick_Handler(void)
{
	/* Raw ticks counter */
	ticks++;

	/* Time counter (sec/ms) */
	if ((++time_ms) == 1000)
	{
		time_s++;
		time_ms = 0;
	}
}
/* EOF */
