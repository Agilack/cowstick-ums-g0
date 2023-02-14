/**
 * @file  hardware.h
 * @brief Headers and definitions for low-level hardware handling
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
#ifndef HARDWARE_H
#define HARDWARE_H
#include "types.h"

/* Peripherals mapped on AHB bus */
#define DMA1   0x40020000
#define DMA2   0x40020400
#define DMAMUX 0x40020800
#define RCC    0x40021000
#define EXTI   0x40021800
#define FLASH  0x40022000
#define CRC    0x40023000
#define RNG    0x40025000
#define AES    0x40026000
/* Peripherals mapped on APB bus */
#define TIM2   0x40000000
#define TIM3   0x40000400
#define TIM4   0x40000800
#define TIM6   0x40001000
#define TIM7   0x40001400
#define TIM14  0x40002000
#define RTC    0x40002800
#define WWDG   0x40002C00
#define IWDG   0x40003000
#define SPI2   0x40003800
#define SPI3   0x40003C00
#define USART2 0x40004400
#define SPI1   0x40013000
/* Ports mapped on IOPORT bus */
#define GPIOA  0x50000000
#define GPIOB  0x50000400
#define GPIOC  0x50000800
#define GPIOD  0x50000C00
#define GPIOE  0x50001000
#define GPIOF  0x50001400

#define GPIO_MODER(x)   (x + 0x00)
#define GPIO_OTYPER(x)  (x + 0x04)
#define GPIO_OSPEEDR(x) (x + 0x08)
#define GPIO_PUPDR(x)   (x + 0x0C)
#define GPIO_IDR(x)     (x + 0x10)
#define GPIO_ODR(x)     (x + 0x14)
#define GPIO_BSRR(x)    (x + 0x18)
#define GPIO_LCKR(x)    (x + 0x1C)
#define GPIO_AFRL(x)    (x + 0x20)
#define GPIO_AFRH(x)    (x + 0x24)
#define GPIO_BRR(x)     (x + 0x28)

#define RCC_IOPRSTR (RCC + 0x24)
#define RCC_IOPENR  (RCC + 0x34)
#define RCC_APBENR1 (RCC + 0x3C)
#define RCC_APBENR2 (RCC + 0x40)

void hw_init(void);


/* -------------------------------------------------------------------------- */
/*                        Low level register functions                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Write a 32 bits value to a memory mapped register
 *
 * @param reg   Address of the register to update
 * @param value New value to write into the register
 */
inline void reg_wr(u32 addr, u32 value)
{
	*(volatile u32 *)addr = value;
}

/**
 * @brief Write a 16 bits value to a memory mapped register
 *
 * @param reg   Address of the register to update
 * @param value New value to write into the register
 */
inline void reg16_wr(u32 addr, u16 value)
{
	*(volatile u16 *)addr = value;
}

/**
 * @brief Write a 8 bits value to a memory mapped register
 *
 * @param reg   Address of the register to update
 * @param value New value to write into the register
 */
inline void reg8_wr(u32 addr, u8 value)
{
	*(volatile u8 *)addr = value;
}

/**
 * @brief Read the value of a 32 bits memory mapped register
 *
 * @param  reg Address of the register to read
 * @return u32 Value of the register (32 bits)
 */
inline u32 reg_rd(u32 reg)
{
	return( *(volatile u32 *)reg );
}

/**
 * @brief Read the value of a 16 bits memory mapped register
 *
 * @param  addr Address of the register to read
 * @return u16  Value of the register (16 bits)
 */
inline u16 reg16_rd(u32 addr)
{
	return( *(volatile u16 *)addr );
}

/**
 * @brief Read the value of a 8 bits memory mapped register
 *
 * @param  addr Address of the register to read
 * @return u8   Value of the register
 */
inline u8 reg8_rd(u32 addr)
{
	return( *(volatile u8 *)addr );
}

/**
 * @brief Modify a 32 bits memory mapped register by clearing some bits
 *
 * @param addr  Address of the register to write
 * @param value Mask of the bits to clear
 */
inline void reg_clr(u32 addr, u32 value)
{
	*(volatile u32 *)addr = ( *(volatile u32 *)addr & ~value );
}

/**
 * @brief Modify a 16 bits memory mapped register by clearing some bits
 *
 * @param addr  Address of the register to write
 * @param value Mask of the bits to clear
 */
inline void reg16_clr(u32 addr, u16 value)
{
	*(volatile u16 *)addr = ( *(volatile u16 *)addr & ~value );
}
/**
 * @brief Modify a 8 bits memory mapped register by clearing some bits
 *
 * @param addr  Address of the register to write
 * @param value Mask of the bits to clear
 */
inline void reg8_clr(u32 addr, u8 value)
{
	*(volatile u8 *)addr = ( *(volatile u8 *)addr & ~value );
}

/**
 * @brief Modify a 32 bits memory mapped register by setting some bits
 *
 * @param addr  Address of the register to write
 * @param value Mask of the bits to set
 */
inline void reg_set(u32 addr, u32 value)
{
	*(volatile u32 *)addr = ( *(volatile u32 *)addr | value );
}

/**
 * @brief Modify a 16 bits memory mapped register by setting some bits
 *
 * @param addr  Address of the register to modify
 * @param value Mask of the bits to set
 */
inline void reg16_set(u32 addr, u16 value)
{
	*(volatile u16 *)addr = ( *(volatile u16 *)addr | value );
}

/**
 * @brief Modify a 8 bits memory mapped register by setting some bits
 *
 * @param addr  Address of the register to modify
 * @param value Mask of the bits to set
 */
inline void reg8_set(u32 addr, u8 value)
{
	*(volatile u8 *)addr = ( *(volatile u8 *)addr | value );
}
#endif
