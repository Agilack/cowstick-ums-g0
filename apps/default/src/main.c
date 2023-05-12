/**
 * @file  main.c
 * @brief Entry point of the firmware and main application loop
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
#include "log.h"
#include "time.h"

#define API_BASE 0x080000D0

static void api_init(void);

static u32 tm_ref;

/**
 * @brief App initialization handler
 *
 * This function is referenced into application vector table and called by
 * the main firmware on startup. It is always the first application function
 * called.
 */
int app_init(void)
{
	api_init();

	log_print(LOG_INF, "APP: Default app initialized\n");
	tm_ref = time_now(0);

	// Success
	return(0);
}

/**
 * @brief App periodic process
 *
 * This function is referenced into application vector table and called by
 * the main firmware on each cycle of the main firmware loop to do periodic
 * stuff.
 */
void app_periodic(void)
{
	if (time_since(tm_ref) > 4000)
	{
		log_print(LOG_DBG, "APP: %{Periodic BEEP :p%}\n", LOG_BCYN);
		tm_ref = time_now(0);
	}
}

/**
 * @brief App reset
 *
 * This function is referenced into application vector table and called by
 * the main firmware when USB device is reset.
 */
void app_reset(void)
{
	log_print(LOG_DBG, "APP: %{Reseted%}\n", LOG_YLW);
}

/* API functions used by the app */
void (*log_print)(uint level, const char *s, ...);
u32  (*time_now)(tm_t *timeval); 
int  (*time_since)(u32 ref);

/**
 * @brief Initialize API mapped functions
 *
 * This function read tables of firmware exposed functions and initialize
 * function pointers accordingly.
 */
static void api_init(void)
{
	u32 t_addr;

	t_addr = *(u32*)(API_BASE + 0x08);
	time_now   = (u32(*)(tm_t *))(*(u32*)(t_addr+0x00));
	time_since = (int(*)(u32))   (*(u32*)(t_addr+0x04));
	t_addr = *(u32*)(API_BASE + 0x0C);
	log_print = (void(*)(uint,const char *,...))(*(u32*)(t_addr+0x1C));
}
/* EOF */
