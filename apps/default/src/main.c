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
#include "mem.h"
#include "scsi.h"
#include "time.h"

#undef SCSI_DEBUG_READ

#define API_BASE 0x080000D0

static void api_init(void);
int scsi_rd(u32 addr, u32 len, u8 *data);
int scsi_vendor(lun *unit, u32 *ctx, u8 *cb, uint len);

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
	lun *unit;

	api_init();

	unit = scsi_lun_get(0);
	/* Configure lun callbacks */
	unit->rd         = scsi_rd;
	unit->cmd_vendor = scsi_vendor;
	/* Initialize lun format */
	unit->capacity = 131072;
	unit->state    = 1;
	unit->writable = 0;

	log_print(LOG_INF, "APP: Default app initialized %32x\n", unit);
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

/**
 * @brief Read function for the LUN
 *
 * This function is registered as read handler for the SCSI lun 0 and called
 * by the SCSI layer when a read request is received.
 *
 * @param addr Address to read
 * @param len  Number of byte to read
 * @param data Pointer to a buffer where readed data can be stored
 * @return integer Number of readed bytes
 */
int scsi_rd(u32 addr, u32 len, u8 *data)
{
	if (len > 512)
		len = 512;

#ifdef SCSI_DEBUG_READ
	log_print(LOG_DBG, "APP: SCSI Read %d bytes at 0x%32x\n", len, addr);
#endif

	mem_read(0, addr, len, data);

	return((int)len);
}

/**
 * @brief Handle a vendor command for the LUN
 *
 * This function is registered as a callback for SCSI lun 0 and is called by
 * the SCSI layer when a vendor command (group 6 or group7) is received.
 *
 * @param unit Pointer to the LUN structure where command has been received
 * @param ctx Pointer to a "context" variable used to track transactions
 * @param cb  Pointer to the received command block
 * @param len Length of the received command
 * @return integer Result of the command.
 */
int scsi_vendor(lun *unit, u32 *ctx, u8 *cb, uint len)
{
	(void)unit;
	(void)ctx;
	(void)len;

	log_print(LOG_DBG, "APP: SCSI vendor command %8x\n", cb[0]);
	return(0);
}

/* API functions used by the app */
void (*log_print)(uint level, const char *s, ...);
u32  (*time_now)(tm_t *timeval); 
int  (*time_since)(u32 ref);
int  (*mem_read) (uint nid, u32 addr, uint len, u8 *buffer);
lun *(*scsi_lun_get)(int pos);

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
	t_addr = *(u32*)(API_BASE + 0x10);
	mem_read = (int(*)(uint,u32,uint,u8*))(*(u32*)(t_addr+0x04));
	t_addr = *(u32*)(API_BASE + 0x14);
	scsi_lun_get = (lun*(*)(int))(*(u32*)(t_addr + 0x00));
}
/* EOF */
