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
#include "spi.h"
#include "uart.h"

/**
 * @brief Entry point of the C code
 *
 */
int main(void)
{
	int i;

	/* Initialize low-level hardware */
	hw_init();
	/* Initialize peripherals */
	uart_init();
	spi_init();

	uart_puts("--=={ Cowstick UMS }==--\r\n");

	uart_puts("Detect memory slot #1 : ");
	spi_cs(1, 1);
	spi_rw(1, 0x9F); /* Read DeviceID */
	uart_puthex(spi_rw(1, 0x00), 8);
	uart_puthex(spi_rw(1, 0x00), 8);
	uart_puthex(spi_rw(1, 0x00), 8);
	spi_cs(1, 0);
	uart_puts("\r\n");

	uart_puts("Detect memory slot #2 : ");
	spi_cs(2, 1);
	spi_rw(2, 0x9F); /* Read DeviceID */
	uart_puthex(spi_rw(2, 0x00), 8);
	uart_puthex(spi_rw(2, 0x00), 8);
	uart_puthex(spi_rw(2, 0x00), 8);
	spi_cs(2, 0);
	uart_puts("\r\n");

	uart_puts("Detect memory slot #3 : ");
	spi_cs(3, 1);
	spi_rw(3, 0x9F); /* Read DeviceID */
	uart_puthex(spi_rw(3, 0x00), 8);
	uart_puthex(spi_rw(3, 0x00), 8);
	uart_puthex(spi_rw(3, 0x00), 8);
	spi_cs(3, 0);
	uart_puts("\r\n");

	/* LED blink infinite loop */
	while(1)
	{
		/* LED off */
		reg_wr(GPIOB + 0x18, (1 << 5));
		/* Wait (long) */
		for (i = 0; i < 0x100000; i++)
			asm volatile("nop");
		/* LED on */
		reg_wr(GPIOB + 0x18, (1 << 21));
		/* Wait (short) */
		for (i = 0; i < 0x20000; i++)
			asm volatile("nop");
	}
}
/* EOF */
