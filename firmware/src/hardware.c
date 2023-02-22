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

static inline void _init_clocks(void);
static inline void _init_led(void);
static inline void _init_spi(void);
static inline void _init_uart(void);
static inline void _init_usb(void);

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

	_init_clocks();

	_init_led();
	_init_uart();
	_init_spi();
	_init_usb();
}

/**
 * @brief Configure clocks for main speed operations
 *
 * The clocks are configured as is:
 *   - Processor and SYSCLK run at 64 MHz (using HSI16 and PLL)
 *   - The debug port (USART2) use HSI16
 *   - USB device controller use internal RC oscillator HSI48
 *   - An optional low speed clock for RTC using LSE
 */
static inline void _init_clocks(void)
{
	u32 val;

	/* Set HSI16 as USART2 clock source */
	val = reg_rd(RCC_CCIPR);
	val &= ~(u32)(3 << 2);
	val |=  (u32)(2 << 2);
	reg_wr(RCC_CCIPR, val);

	/* If PLL is used, SYSCLK = 64MHz else SYSCLK = 16 MHz */
#ifdef USE_PLL
	/* Configure PLL */
	val =  (2 << 0);    // PLL clock source = HSI16
	val |= (1 << 4);    // set M (VCO divisor) VCO clock input = 8MHz
	val |= (16 <<  8);  // set N (VCO multiplier) VCO output = 128MHz
	val |= ( 1 << 29);  // set R output divisor, PLLRCLK output = 64MHz
	reg_wr(RCC_PLL_CFGR, val);
	/* Activate PLL and wait for PLL ready */
	reg_set(RCC_CR, (1 << 24));
	while( (reg_rd(RCC_CR) & (1 << 25)) == 0)
		;

	/* Set Flash latency to 2WS */
	reg_set((u32)(FLASH + 0x00), 2); // Flash ACR
	while( (reg_rd(FLASH + 0x00) & 7) != 2)
		;

	/* Enable PLLRCLK */
	reg_set(RCC_PLL_CFGR, (1 << 28));

	/* Change the clock source */
	val  = reg_rd(RCC_CFGR);
	val &= ~(u32)0x07; // Clear System Clock Switch
	val |=  (u32)0x02; // Set PLLRCLK
	reg_wr(RCC_CFGR, val);
#endif

	/* Activate HSI48 RC oscillator */
	reg_set(RCC_CR, (1 << 22));
	while( (reg_rd(RCC_CR) & (1 << 23)) == 0)
		;
	/* Activate CRS */
	reg_set((u32)RCC_APBENR1, (1 << 16));
	/* Reset CRS */
	reg_set((u32)RCC_APBRSTR1, (1 << 16));
	(void)reg_rd((u32)RCC_APBRSTR1);
	reg_clr((u32)RCC_APBRSTR1, (1 << 16));
	/* Configure CRS */
	val = (2 << 28); // SYNC_SRC: USB SOF
	val |= (47999 & 0xFFFF); // Reload value
	val |= ( 22 << 16); // FELIM
	reg_wr(CRS+0x04, val); // CRS_CFGR
	/* Adjust TRIM */
	val = reg_rd(CRS+0x00);
	val &= ~(u32)(0x7F << 8);
	val |=  (u32)(32   << 8);
	reg_wr(CRS+0x00, val);
	/* Enable Auto-Trim and Error counter*/
	reg_set(CRS+0x00, (1 << 6) | (1 << 5));

#ifdef USE_LSE
	/* Activate power controller (PWR) */
	reg_set(RCC_APBENR1, (1 << 28));
	/* Disable RTC domain write protection */
	reg_set(PWR + 0x00, (1 << 8));
	/* Configure LSE drive level */
	reg_set(RCC_BDCR, (3 << 3));
	/* Activate LSE (set LSEON bit) */
	reg_set(RCC_BDCR, (1 << 0));
	for (i = 0; i < 0x100000; i++)
	{
		if (reg_rd(RCC_BDCR) & (1 << 1))
			break;
	}
#endif
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
	u32 speed_a, speed_b;

	/* Read all registers that will be modified for SPI ios */
	moder_a = reg_rd(GPIO_MODER(GPIOA));
	moder_b = reg_rd(GPIO_MODER(GPIOB));
	afrl_a  = reg_rd(GPIO_AFRL(GPIOA));
	afrl_b  = reg_rd(GPIO_AFRL(GPIOB));
	afrh_b  = reg_rd(GPIO_AFRH(GPIOB));
	speed_a = reg_rd(GPIO_OSPEEDR(GPIOA));
	speed_b = reg_rd(GPIO_OSPEEDR(GPIOB));

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
	speed_a |=  (u32)(3 << 10);  // Very High Speed
	/* SPI1: Configure MISO signal (use AF) */
	afrl_a &= ~(u32)(0xF << 24); // Reset PA6 AF to AF0
	moder_a &= ~(u32)(3 << 12);  // Clear PA6 mode
	moder_a |=  (u32)(2 << 12);  // Set mode to Alternate Function
	speed_a |=  (u32)(3 << 12);  // Very High Speed
	/* SPI1: Configure MOSI signal (use AF) */
	afrl_a &= ~(u32)(0xF << 28); // Reset PA7 AF to AF0
	moder_a &= ~(u32)(3 << 14);
	moder_a |=  (u32)(2 << 14);  // Set PA7 mode to Alternate Function
	speed_a |=  (u32)(3 << 14);  // Very High Speed
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
	speed_b |=  (u32)(3 << 16); // Very High Speed
	/* SPI2: Configure MISO signal (use AF) */
	afrl_b &= ~(u32)(0xF << 24);
	afrl_b |=  (u32)(  4 << 24); // PB6 use AF4 (SPI2_MISO)
	moder_b &= ~(u32)(3 << 12);
	moder_b |=  (u32)(2 << 12);  // Set PB6 mode to Alternate Function
	speed_b |=  (u32)(3 << 12);  // Very High Speed
	/* SPI2: Configure MOSI signal (use AF) */
	afrl_b &= ~(u32)(0xF << 28);
	afrl_b |=  (u32)(  1 << 28); // PB7 use AF1 (SPI2_MOSI)
	moder_b &= ~(u32)(3 << 14);
	moder_b |=  (u32)(2 << 14);  // Set PB7 mode to Alternate Function
	speed_b |=  (u32)(3 << 14);  // Very High Speed
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
	reg_wr(GPIO_OSPEEDR(GPIOA), speed_a);
	reg_wr(GPIO_OSPEEDR(GPIOB), speed_b);
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

static inline void _init_usb(void)
{
	u32 v;

	/* Activate SYSCFG (to use PA11 PA12) */
	reg_set(RCC_APBENR2, (1 << 0));

#ifdef HW_RESTART
	v = reg_rd(RCC_CCIPR2);
	v &= ~(u32)(3 << 12); // 00 = HSI48
	reg_wr(RCC_CCIPR2, v);
#endif
	/* Configure PA11 and PA12 as analog (USB pins use special functions) */
	v = reg_rd(GPIO_MODER(GPIOA));
	v |=  (u32)( (3 << 22) | (3 << 24) );
	reg_wr(GPIO_MODER(GPIOA), v);
}
/* EOF */
