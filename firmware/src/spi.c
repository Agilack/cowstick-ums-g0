/**
 * @file  spi.c
 * @brief This file contains SPI driver for STM32G0 USARTs
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
#include "types.h"
#include "spi.h"

/**
 * @brief Initialize SPI interfaces
 *
 * Activate and initialize SPI ports used for external memories. This function
 * must be called before using any other spi functions.
 */
void spi_init(void)
{
	u32 val;

	/* Activate SPI1 */
	reg_set((u32)RCC_APBENR2, (1 << 12));
	/* Activate SPI2 */
	reg_set((u32)RCC_APBENR1, (1 << 14));

	/* Configure SPI to work as master */
	val  = (7 << 3); // Baudrate = f/256
	val |= (1 << 9); // Software Slave Management
	val |= (1 << 8); // SSI
	val |= (1 << 2); // Master mode
	reg16_wr(SPI_CR1(SPI1), val);
	reg16_wr(SPI_CR1(SPI2), val);

	/* Configure format for external memory communication */
	val =  (7 <<  8); // Data Size: 8 bits
	val |= (1 << 12); // FRXTH: Reception threshold (1 byte)
	reg16_wr(SPI_CR2(SPI1), val);
	reg16_wr(SPI_CR2(SPI2), val);

	/* Enable SPI1 */
	reg16_set(SPI_CR1(SPI1), (1 << 6));
	/* Enable SPI2 */
	reg16_set(SPI_CR1(SPI2), (1 << 6));

	/* Disable Hold signals (allow devices to communicate) */
	reg_wr(GPIO_BSRR(GPIOA), (1 << 8)); // SPI1 Hold = 1
	reg_wr(GPIO_BSRR(GPIOB), (1 << 3)); // SPI2 Hold = 1
}

/**
 * @brief Enable or disable SPI channel
 *
 * @param channel SPI channel
 * @param state   Disable or enable
 */
void spi_cs(uint channel, int state)
{
	switch (channel)
	{
		/* SPI1 channel 1 */
		case 1:
			if (state)
				reg_wr(GPIO_BSRR(GPIOA), (1 << 20));
			else
				reg_wr(GPIO_BSRR(GPIOA), (1 <<  4));
			break;
		/* SPI1 channel 2 */
		case 2:
			if (state)
				reg_wr(GPIO_BSRR(GPIOB), (1 << 17));
			else
				reg_wr(GPIO_BSRR(GPIOB), (1 <<  1));
			break;
		/* SPI2 */
		case 3:
			if (state)
				reg_wr(GPIO_BSRR(GPIOB), (1 << 25));
			else
				reg_wr(GPIO_BSRR(GPIOB), (1 <<  9));
			break;
		/* Unknown channel */
		default:
			// TODO Notify error ?
			break;
	}
}

u8 spi_rw(uint channel, u8 out)
{
	u32 port;
	int i;

	if ((channel == 1) || (channel == 2))
		port = SPI1;
	else if (channel == 3)
		port = SPI2;
	else
		return(0);

	reg8_wr(SPI_DR(port), out);
	/* Wait for RX */
	for (i = 0; i < 0x100000; i++)
	{
		if (reg16_rd(SPI_SR(port)) & (1 << 0))
			break;
	}
	return(reg8_rd(SPI_DR(port)));
}
/* EOF */
