/**
 * @file  hardware.c
 * @brief Low-level cowstick-ums hardware configuration
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

static inline void _init_led(void);
static inline void _init_spi(void);
static inline void _init_uart(void);

/**
 * @brief Initialize processor, clocks and some peripherals
 *
 * This function should be called on startup for clocks and IOs configuration.
 */
void hw_init(void)
{
	int i;

	/* RCC : Activate GPIOA and GPIOB */
	reg_wr(RCC_IOPENR, (1 << 1) | (1 << 0));
	/* RCC : Reset GPIOA and GPIOB */
	reg_wr(RCC_IOPRSTR, (1 << 1) | (1 << 0));
	for (i = 0; i < 16; i++)
		asm volatile("nop");
	reg_wr(RCC_IOPRSTR, 0);

	_init_led();
	_init_uart();
	_init_spi();
}

/**
 * @brief Initialize the IO connected to LED
 *
 */
static inline void _init_led(void)
{
	u32 v;

	/* Use open-drain io for LED */
	reg_wr(GPIO_OTYPER(GPIOB), (1 << 5));
	/* Default state : IO=1 -> LED off */
	reg_wr(GPIO_BSRR(GPIOB),   (1 << 5));
	/* Set PB5 as general purpose output */
	v = reg_rd(GPIO_MODER(GPIOB));
	v &= ~(u32)(3 << 10);
	v |=  (u32)(1 << 10);
	reg_wr(GPIO_MODER(GPIOB), v);
}

/**
 * @brief Initialize the IOs connected to SPI memories
 *
 */
static inline void _init_spi(void)
{
	u32 moder_a, moder_b;
	u32 afrl_a,  afrl_b, afrh_b;

	/* Read all registers that will be modified for SPI ios */
	moder_a = reg_rd(GPIO_MODER(GPIOA));
	moder_b = reg_rd(GPIO_MODER(GPIOB));
	afrl_a  = reg_rd(GPIO_AFRL(GPIOA));
	afrl_b  = reg_rd(GPIO_AFRL(GPIOB));
	afrh_b  = reg_rd(GPIO_AFRH(GPIOB));

	/* SPI1: Configure Chip Select 1 (PA4) as output */
	reg_wr(GPIO_BSRR(GPIOA), (1 << 4));
	moder_a &= ~(u32)(3 << 8);
	moder_a |=  (u32)(1 << 8);
	/* SPI1: Configure Chip Select 2 (PB1) as output */
	reg_wr(GPIO_BSRR(GPIOB), (1 << 1));
	moder_b &= ~(u32)(3 << 2);
	moder_b |=  (u32)(1 << 2);
	/* SPI2: Configure Chip Select (PB9) as output */
	reg_wr(GPIO_BSRR(GPIOB), (1 << 9));
	moder_b &= ~(u32)(3 << 18);
	moder_b |=  (u32)(1 << 18);
	/* Apply MODER now to force all memory chips to inactive (CS=1) */
	reg_wr(GPIO_MODER(GPIOA), moder_a);
	reg_wr(GPIO_MODER(GPIOB), moder_b);

	/* SPI1: Configure SCK signal (use AF) */
	afrl_a &= ~(u32)(0xF << 20); // Reset PA5 AF to AF0
	moder_a &= ~(u32)(3 << 10);  // Clear PA5 mode
	moder_a |=  (u32)(2 << 10);  // Set mode to Alternate Function
	/* SPI1: Configure MISO signal (use AF) */
	afrl_a &= ~(u32)(0xF << 24); // Reset PA6 AF to AF0
	moder_a &= ~(u32)(3 << 12);  // Clear PA6 mode
	moder_a |=  (u32)(2 << 12);  // Set mode to Alternate Function
	/* SPI1: Configure MOSI signal (use AF) */
	afrl_a &= ~(u32)(0xF << 28); // Reset PA7 AF to AF0
	moder_a &= ~(u32)(3 << 14);
	moder_a |=  (u32)(2 << 14);  // Set PA7 mode to Alternate Function
	/* SPI1: Congigure Hold signal (as GPIO) */
	reg_wr(GPIO_BSRR(GPIOA), (1 << 24));
	moder_a &= ~(u32)(3 << 16);
	moder_a |=  (u32)(1 << 16);  // Set PA8 mode to GP output
	/* SPI1: Configure Write Protect signal (as GPIO) */
	reg_wr(GPIO_BSRR(GPIOB), (1 << 18)); // WP=0 (proction active)
	moder_b &= ~(u32)(3 << 4);
	moder_b |=  (u32)(1 << 4);   // Set PB2 mode to GP output

	/* SPI2: Configure SCK signal (use AF) */
	afrh_b &= ~(u32)(0xF << 0);
	afrh_b |=  (u32)(  1 << 0); // PB8 use AF1
	moder_b &= ~(u32)(3 << 16);
	moder_b |=  (u32)(2 << 16); // Set PB8 mode to Alternate Function
	/* SPI2: Configure MISO signal (use AF) */
	afrl_b &= ~(u32)(0xF << 24);
	afrl_b |=  (u32)(  4 << 24); // PB6 use AF4 (SPI2_MISO)
	moder_b &= ~(u32)(3 << 12);
	moder_b |=  (u32)(2 << 12);  // Set PB6 mode to Alternate Function
	/* SPI2: Configure MOSI signal (use AF) */
	afrl_b &= ~(u32)(0xF << 28);
	afrl_b |=  (u32)(  1 << 28); // PB7 use AF1 (SPI2_MOSI)
	moder_b &= ~(u32)(3 << 14);
	moder_b |=  (u32)(2 << 14);  // Set PB7 mode to Alternate Function
	/* SPI2: Configure Hold signal (as GPIO) */
	reg_wr(GPIO_BSRR(GPIOB), (1 << 19)); // WP=0 (proction active)
	moder_b &= ~(u32)(3 << 6);
	moder_b |=  (u32)(1 << 6);  // Set PB3 mode to GP output
	/* SPI2: Configure Write Protect signal (as GPIO) */
	reg_wr(GPIO_BSRR(GPIOB), (1 << 20));
	moder_b &= ~(u32)(3 << 8);
	moder_b |=  (u32)(1 << 8);  // Set PB4 mode to GP output

	/* Apply all registers changes */
	reg_wr(GPIO_AFRL(GPIOA), afrl_a);
	reg_wr(GPIO_AFRL(GPIOB), afrl_b);
	reg_wr(GPIO_AFRH(GPIOB), afrh_b);
	reg_wr(GPIO_MODER(GPIOA), moder_a);
	reg_wr(GPIO_MODER(GPIOB), moder_b);
}

/**
 * @brief Initialize IOs of the (debug) uart port
 *
 */
static inline void _init_uart(void)
{
	u32 v;

	/* Configure PA2 (UART-TX) and PA3 (UART-RX) to use AF1 (USART2) */
	v = reg_rd(GPIO_AFRL(GPIOA));
	v &= ~(u32)(0xF <<  8); // Clear current PA2 config
	v |=  (u32)(  1 <<  8); // PA2 use AF1
	v &= ~(u32)(0xF << 12); // Clear current PA3 config
	v |=  (u32)(  1 << 12); // PA3 use AF1
	reg_wr(GPIO_AFRL(GPIOA), v);

	/* Configure PA2 and PA3 to use alternate function */
	v = reg_rd(GPIO_MODER(GPIOA));
	v &= ~(u32)( (3 << 4) | (3 << 6) );
	v |=  (u32)( (2 << 4) | (2 << 6) );
	reg_wr(GPIO_MODER(GPIOA), v);
}
/* EOF */
