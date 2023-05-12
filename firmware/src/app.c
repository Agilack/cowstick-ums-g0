/**
 * @file  app.c
 * @brief This module initialize and configure the custom app extension
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
#include "app.h"
#include "log.h"
#include "types.h"

/* Declaration of global custom app exposed functions */
void (*app_periodic)(void);
void (*app_reset)(void);

static int  is_fct_valid(u32 addr);
static void default_periodic(void);
static void default_reset(void);

/**
 * @brief Initialize cutom app
 *
 * This function test the presence of a custom app and register all the
 * callback functions. To work properly, this function must be called
 * before any use of a custom app api.
 */
void app_init(void)
{
	int (*ext_app_init)(void);

	/* Register default handlers */
	app_periodic = default_periodic;
	app_periodic = default_reset;

	/* Test if an app is present by reading signature */
	if (*(u32 *)0x0801000C != 0xBABEFACE)
	{
		log_print(LOG_INF, "APP: No custom app signature found\n");
		return;
	}

	/* Load the custom app vectors */
	ext_app_init = (int (*)(void)) (*(u32 *)0x08010000);
	app_periodic = (void(*)(void)) (*(u32 *)0x08010004);
	app_reset    = (void(*)(void)) (*(u32 *)0x08010008);

	/* Test vector validity, if invalid use default */
	if ( ! is_fct_valid((u32)app_periodic) )
	{
		if ((u32)app_periodic != 0)
			log_print(LOG_WRN, "APP: Invalid periodic function %{%32x%} use default\n", LOG_RED, (u32)app_periodic);
		app_periodic = default_periodic;
	}
	if ( ! is_fct_valid((u32)app_reset) )
	{
		if ((u32)app_reset != 0)
			log_print(LOG_WRN, "APP: Invalid reset function %{%32x%} use default\n", LOG_RED, (u32)app_reset);
		app_reset = default_reset;
	}

	if (is_fct_valid((u32)ext_app_init))
	{
		// Call app initialization function
		if (ext_app_init())
		{
			log_print(LOG_ERR, "APP: Custom app init %{fails%}\n", LOG_RED);
			// App init fails, unregister other functions
			app_periodic = default_periodic;
			app_reset    = default_reset;
		}
	}
	log_print(LOG_INF, "  Vector periodic: %8x\n", app_periodic);
	log_print(LOG_INF, "  Vector reset:    %8x\n", app_reset);
}

/**
 * @brief Test if an address can be valid for a function
 *
 * @param addr Tested address
 * @return boolean True is returned if addres is valid
 */
static int is_fct_valid(u32 addr)
{
	/* If address if before flash start : invalid */
	if (addr < 0x08000000)
		return(0);
	/* If address is after flash end : invalid */
	if (addr > 0x0801FFFF)
		return(0);

	/* Address can be a valid function */
	return(1);
}

/**
 * @brief Default periodic handler
 *
 * This dummy function can be used as periodic handler when there is no
 * custom app or when the custom app does not contains a periodic handler.
 */
static void default_periodic(void)
{
	/* Do nothing */
}

/**
 * @brief Default reset handler
 *
 * This dummy function can be used as reset handler when there is no custom
 * app or when the custom app does not contains a reset handler.
 */
static void default_reset(void)
{
	/* Do nothing */
}
/* EOF */
