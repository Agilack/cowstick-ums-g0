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
#include "driver/flash_mcu.h"

void test_mem(void);
void test_flash_mcu(void);

/**
 * @brief Entry point of the C code
 *
 */
int main(void)
{
	u32  tm;
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

#ifdef TEST_FLASH_MCU
	test_flash_mcu();
#endif
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
#ifdef TEST_MEM
	test_mem();
#endif

	/* Start USB device */
	usb_start();

	tm = time_now(0);

	/* Main firmware loop */
	while(1)
	{
		usb_periodic();

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

#ifdef TEST_FLASH_MCU
void test_flash_mcu(void)
{
	u8 buffer[256];
	u32 addr, v;
	int i;

	log_print(LOG_DBG, "Test: Dump initial flash content (bank2) :\n");
	log_dump((u8 *)0x08020000, 64, 1);

	log_print(LOG_DBG, "\nTest: Erase first page of bank2 ...\n");
	flash_mcu_erase(0x08020000);
	for (addr = 0x08020000; addr < 0x08020800; addr += 4)
	{
		v = *(u32 *)addr;
		if (v != 0xFFFFFFFF)
		{
			log_print(LOG_DBG, "Test: %{Erase failed%} at %32x readed %32x\n", 1, addr, v);
			break;
		}
	}
	if (addr == 0x08020800)
		log_print(LOG_DBG, "Test: Page erase %{success%}\n", 2);

	log_print(LOG_DBG, "\nTest: Write content to page\n");
	// Prepare bufffer with values
	for (i = 0; i < 0xFF; i++)
		buffer[i] = (u8)(i & 0xFF);
	// Write this test pattern to the whole page
	for (addr = 0x08020000; addr < 0x08020800; addr += 256)
		flash_mcu_write(addr, buffer, 256);
	// Try to read back and verify
	for (addr = 0x08020000; addr < 0x08020800; addr++)
	{
		v = *(u8 *)addr;
		if (v != (addr & 0xFF))
		{
			log_print(LOG_DBG, "Test: %{Write failed%} at %32x : expected %8x but readed %8x\n", 1, addr, (addr & 0xFF), v);
			break;
		}
	}
	if (addr == 0x08020800)
		log_print(LOG_DBG, "Test: Page erase %{success%}\n", 2);

	log_print(LOG_DBG, "\nTest: Dump initial flash content (bank1) :\n");
	log_dump((u8 *)0x08010000, 512, 1);

	//flash_mcu_erase(0x08010000);
	//flash_mcu_erase(0x0801F800);
	//flash_mcu_erase(0x0803F800);
	//while(1);
	uart_flush();
}
#endif
/* EOF */
