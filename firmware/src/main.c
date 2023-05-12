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
#include "app.h"
#include "hardware.h"
#include "libc.h"
#include "log.h"
#include "mem.h"
#include "scsi.h"
#include "spi.h"
#include "time.h"
#include "uart.h"
#include "usb.h"
#include "usb_msc.h"

int default_lun_rd(u32 addr, u32 len, u8 *data);
int default_lun_wr(u32 addr, u32 len, u8 *data);
int default_lun_wr_complete(void);
int default_lun_wr_preload(u32 addr);
void test_mem(void);

/**
 * @brief Entry point of the C code
 *
 */
int main(void)
{
	lun *scsi_lun;
	u32  tm;
	u32  tm_ref;
	uint i;

	/* Initialize low-level hardware */
	hw_init();
	time_init();
	/* Initialize peripherals */
	uart_init();
	spi_init();
	usb_init();

	log_print(0, "--=={ Cowstick UMS }==--\r\n");

	/* Initialize libraries */
	log_init();
	mem_init();
	scsi_init();
	usb_msc_init();

	/* Initialize and start custom app (if any) */
	app_init();

	/* Insert an empry log line after end of inits */
	log_print(LOG_INF, "\n");

	mem_detect();
	for (i = 0; i < MEM_NODE_COUNT; i++)
	{
		mem_node *node = mem_get_node(i);
		if (node == 0)
			break;
		log_print(LOG_INF, "Memory slot #%d : ", i);
		if (node->type == 0)
			log_print(LOG_INF, "Empty\n");
		else if (node->type == 1)
		{
			const mem_flash_chip *fc;
			fc = (const mem_flash_chip *)node->chip;
			log_print(LOG_INF, "FLASH %s\n", fc->name);
		}
	}
	/* Configure default SCSI LUN */
	scsi_lun = scsi_lun_get(0);
	scsi_lun->state = 0;
	scsi_lun->rd    = default_lun_rd;
	scsi_lun->wr    = default_lun_wr;
	scsi_lun->wr_complete = default_lun_wr_complete;
	scsi_lun->wr_preload  = default_lun_wr_preload;
	/* Start USB device */
	usb_start();

#ifdef TEST_MEM
	test_mem();
#endif
	tm = time_now(0);
	tm_ref = tm;

	/* Main firmware loop */
	while(1)
	{
		usb_periodic();

		scsi_lun = scsi_lun_get(0);
		if (scsi_lun->state == 0)
		{
			if (time_since(tm_ref) > 10000)
			{
				uart_puts("Main: Mark SCSI medium as inserted\r\n");
				// 131072 blocks (64MB)
				scsi_lun->capacity = 131072;
				scsi_lun->state = 1;
				scsi_lun->writable = 1;
			}
		}

		app_periodic();

		/* Blink led1 */
		if (time_since(tm) > 400)
		{
			if (reg_rd(GPIOB + 0x10) & (1 << 5))
				/* Set LED on */
				reg_wr(GPIOB + 0x18, (1 << 21));
			else
				/* Set LED off */
				reg_wr(GPIOB + 0x18, (1 << 5));
			tm = time_now(0);
		}
	}
}

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

#ifdef TEST_MEM
void test_mem(void)
{
	mem_node *node = mem_get_node(0);
	uint i;

	log_print(0, "read() result=%d\n",
	    (uint)mem_read(0, 0x000000, 512, 0) );
	log_dump(node->cache_buffer, 64, 2);

	memset(node->cache_buffer, 0, 4096);
	mem_write(0, 0x000000, 512, 0);

	log_print(0, "read() result=%d\n",
	    (uint)mem_read(0, 0x000000, 512, 0) );
	log_dump(node->cache_buffer, 64, 2);

	for (i = 0; i < 16; i++)
	{
		if (i & 1)
			node->cache_buffer[i] = 0x55;
		else
			node->cache_buffer[i] = 0xAA;
	}
	mem_write(0, 0x000000, 512, 0);

	log_print(0, "read() result=%d\n",
	    (uint)mem_read(0, 0x000000, 512, 0) );
	log_dump(node->cache_buffer, 64, 2);

	mem_erase(0, 0x000000, 512);

	log_print(0, "read() result=%d\n",
	    (uint)mem_read(0, 0x000000, 512, 0) );
	log_dump(node->cache_buffer, 64, 2);
	while(1);
}
#endif
/* EOF */
