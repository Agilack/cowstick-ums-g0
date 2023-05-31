/**
 * @file  driver/flash_mcu.h
 * @brief Headers and definitions for STM32G0 internal flash driver
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
#ifndef FLASH_MCU_H
#define FLASH_MCU_H
#include "hardware.h"

#define FLASH_KEYR (FLASH + 0x008)
#define FLASH_SR   (FLASH + 0x010)
#define FLASH_CR   (FLASH + 0x014)

int  flash_mcu_erase(unsigned int addr);
void flash_mcu_lock(void);
int  flash_mcu_unlock(void);
int  flash_mcu_write(u32 addr, u8 *data, int len);

#endif
