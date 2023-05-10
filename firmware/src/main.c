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
#include "hardware.h"
#include "libc.h"
#include "log.h"
#include "mem.h"
#include "spi.h"
#include "time.h"
#include "uart.h"
#include "usb.h"
#include "usb_msc.h"

void test_mem(void);

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
	/* Initialize libraries */
	log_init();
	mem_init();
	usb_msc_init();

	log_print(0, "--=={ Cowstick UMS }==--\r\n");

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
	usb_start();

#ifdef TEST_MEM
	test_mem();
#endif
	tm = time_now(0);
	/* LED blink infinite loop */
	while(1)
	{
		usb_periodic();

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
/* EOF */
