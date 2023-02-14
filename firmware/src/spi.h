/**
 * @file  spi.h
 * @brief Headers and definitions for STM32G0 SPI driver
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
#ifndef SPI_H
#define SPI_H
#include "hardware.h"

#define SPI_CR1(x)     (x + 0x00)
#define SPI_CR2(x)     (x + 0x04)
#define SPI_SR(x)      (x + 0x08)
#define SPI_DR(x)      (x + 0x0C)
#define SPI_CRCPR(x)   (x + 0x10)
#define SPI_RXCRCR(x)  (x + 0x14)
#define SPI_TXCRCR(x)  (x + 0x18)
#define SPI_I2SCFGR(x) (x + 0x1C)
#define SPI_I2SPR(x)   (x + 0x20)

void spi_init(void);
void spi_cs(uint channel, int state);
u8   spi_rw(uint channel, u8 out);

#endif
