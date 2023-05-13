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
#include "libc.h"
#include "log.h"
#include "mem.h"
#include "scsi.h"
#include "time.h"
#include "types.h"

/* Declaration of global custom app exposed functions */
void (*app_periodic)(void);
void (*app_reset)(void);

static int  is_fct_valid(u32 addr);
static void default_init(void);
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
	app_reset    = default_reset;

	/* Test if an app is present by reading signature */
	if (*(u32 *)0x0801000C != 0xBABEFACE)
	{
		log_print(LOG_INF, "APP: No custom app signature found\n");
		default_init();
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
	else
		default_init();

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

/* -------------------------------------------------------------------------- */
/* --                     Default application entries                      -- */
/* -------------------------------------------------------------------------- */

int default_lun_rd(u32 addr, u32 len, u8 *data);
int default_lun_wr(u32 addr, u32 len, u8 *data);
int default_lun_wr_complete(void);
int default_lun_wr_preload(u32 addr);

static u32 app_tm_ref;

/**
 * @brief Default app initialization handler
 *
 * This function is called on startup when there is no custom app or when the
 * custom app have no init function. To keep compatibility with previous version
 * and allow to work without custom firmware, a basic storage is configured.
 */
static void default_init(void)
{
	lun *scsi_lun;

	app_tm_ref = time_now(0);

	/* Configure default SCSI LUN */
	scsi_lun = scsi_lun_get(0);
	scsi_lun->state = 0;
	scsi_lun->rd    = default_lun_rd;
	scsi_lun->wr    = default_lun_wr;
	scsi_lun->wr_complete = default_lun_wr_complete;
	scsi_lun->wr_preload  = default_lun_wr_preload;
}

/**
 * @brief Default periodic handler
 *
 * This dummy function can be used as periodic handler when there is no
 * custom app or when the custom app does not contains a periodic handler.
 */
static void default_periodic(void)
{
	lun *scsi_lun;

	scsi_lun = scsi_lun_get(0);
	if (scsi_lun->state == 0)
	{
		if (time_since(app_tm_ref) > 10000)
		{
			log_puts("Main: Mark SCSI medium as inserted\n");
			// 131072 blocks (64MB)
			scsi_lun->capacity = 131072;
			scsi_lun->state = 1;
			scsi_lun->writable = 1;
		}
	}

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

/* -------------------------------------------------------------------------- */
/* --  -- */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read function for the default LUN
 *
 * This function is registered as read handler for the SCSI lun 0 and called
 * by the SCSI layer when a read request is received.
 *
 * @param addr Address to read
 * @param len  Number of byte to read
 * @param data Pointer to a buffer where readed data can be stored
 * @return integer Number of readed bytes
 */
int default_lun_rd(u32 addr, u32 len, u8 *data)
{
	if (len > 512)
		len = 512;

#ifdef LUN_DEBUG_READ
	log_print(LOG_DBG, "LUN: Read %d bytes at 0x%32x\n", len, addr);
#endif

	mem_read(0, addr, len, data);

	return((int)len);
}

/**
 * @brief Write function for the default LUN
 *
 * @param addr Address to write
 * @param len  Number of bytes to write
 * @param data Pointer to a buffer with data to write
 * @return integer Zero is returned on success, other values are errors
 */
int default_lun_wr(u32 addr, u32 len, u8 *data)
{
	mem_node *node = mem_get_node(0);

	(void)len;

	if ((addr & 0xFFFFF000) != node->cache_addr)
	{
#ifdef LUN_DEBUG_WRITE
		log_print(LOG_INF, "LUN: Write, cache new page %32x\n", addr);
#endif
		mem_write(0, 0, 0, 0);
		mem_read(0, addr, 512, 0);
	}
#ifdef LUN_DEBUG_WRITE
	log_print(LOG_INF, "LUN: Write at %32x\n", addr);
#endif
	memcpy(node->cache_buffer + (addr & 0xFFF), data, 512);
	return(0);
}

/**
 * @brief Write complete function for the default LUN
 *
 * This function is registered as handler for the SCSI lun 0 and called by
 * the SCSI layer when a write transaction is completed. This can be usefull
 * for example to flush the last written bytes.
 *
 * @return integer Zero is returned on success, other values are errors
 */
int default_lun_wr_complete(void)
{
	mem_write(0, 0, 0, 0);
	return(0);
}

/**
 * @brief Write preload function for the default LUN
 *
 * This function is registered as handler for the SCSI lun 0 and called by
 * the SCSI layer at the begining of a write transaction. This can be used
 * for example to preload a cache.
 *
 * @param addr First accessed address of the write transaction
 */
int default_lun_wr_preload(u32 addr)
{
	mem_read(0, addr, 512, 0);
	return(0);
}

/* EOF */
