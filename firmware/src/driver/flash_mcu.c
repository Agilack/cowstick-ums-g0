/**
 * @file  driver/flash_mcu.c
 * @brief This file contains driver for STM32G0 internal flash manipulations
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
#include "driver/flash_mcu.h"
#include "hardware.h"
#include "log.h"
#include "types.h"
#include "uart.h"

#define FLASH_MCU_DEBUG
#define ERASE_RAMFUNC

/**
 * @brief Erase one page into embedded flash
 *
 * This function can be used to erase embedded flash.
 * WARNING: It is not  possible to erase a page when code is executed from
 * same bank. To avoid this limitation, erase function can be moved to RAM
 * with the ERASE_RAMFUNC directive.
 *
 * @param addr Adresse of the page to erase (relative of absolute)
 */
#ifdef ERASE_RAMFUNC
__attribute__ ((section(".ramfunc")))
#endif
int flash_mcu_erase(unsigned int addr)
{
	uint sz, bk, page, v;
	int  i;

	// Keep only lower bits (if absolute address is used)
	addr &= 0x000FFFFF;

	// Verify that specified address is into device flash
	sz = reg16_rd(0x1FFF75E0); // Flash memory size data register (in kB)
	if (addr >= (sz * 1024) )
		return(-1);

	// TODO Add test for allowed erase regions

	// Compute page number from address
	page = (addr / 2048);
	bk = 0;
	// For MCU with 2 banks, translate page numbers
	if ( (sz == 0x100) && (page > 63) )
	{
		page = (page - 64 + 256);
		bk = (1 << 13);
	}
	else if ( (sz == 0x200) && (page > 127))
	{
		page = (page - 127 + 256);
		bk = (1 << 13);
	}

	// Unlock memory before erasing
	if (flash_mcu_unlock())
		return(-2);

	// Configure erase operation
	v  = (1 << 1);              // PER: Page Erase
	v |= ((page & 0x3FF) << 3); // PNB: Page number
	v |= bk;                    // BKER: Bank selection
	reg_wr(FLASH_CR, v);
	// Start erase operation
	v |= (1 << 16); // Set STRT bit
	reg_wr(FLASH_CR, v);

	// Wait end of operation
	for (i = 0; i < 0x40000000; i++)
	{
		v = reg_rd(FLASH_SR);
		if (bk && (v & (2 << 16)))
			continue;
		else if (v & (1 << 16))
			continue;
		break;
	}

	// Clear used bits (PER, PNB, ...)
	reg_wr(FLASH_CR, 0);
	// Re-enable protection
	flash_mcu_lock();

#ifdef FLASH_MCU_DEBUG
	log_print(LOG_DBG, "Flash: Page erased at %32x after %d cycles. SR=%32x\n", addr, i, v);
#endif

	return(0);
}

/**
 * @brief Enable flash CR register protection
 *
 */
void flash_mcu_lock(void)
{
	u32 v;

	v = reg_rd(FLASH_CR);
	/* If the LOCK bit is set, nothing to do */
	if (v & (u32)(1 << 31))
		return;

	/* Set the lock bit ... */
	v |= (u32)(1 << 31);
	/* ... and write back control register */
	reg_wr(FLASH_CR, v);
}

/**
 * @brief Unlock flash memory to allow erase or write access (DANGEROUS)
 *
 * @return integer Return zero for success, other values are error code
 */
int flash_mcu_unlock(void)
{
	/* If the LOCK bit is clear, nothing to do */
	if ((reg_rd(FLASH_CR) & (u32)(1 << 31)) == 0)
		return(0);

	/* Write unlock sequence */
	reg_wr(FLASH_KEYR, 0x45670123);
	reg_wr(FLASH_KEYR, 0xCDEF89AB);
	/* Test if memory unlocked */
	if ( reg_rd(FLASH_CR) & (u32)(1 << 31) )
		return(-1); /* No :( Fatal error */
	
	return(0);
}

/**
 * @brief Write multiple bytes at a specified flash address
 *
 * @param addr Address where buffer must be written
 * @param data Pointer to a buffer with datas to write
 * @param len  Number of bytes to write
 * @return integer Zero is returned on success, other values are errors
 */
int flash_mcu_write(u32 addr, u8 *data, int len)
{
	u32 v;
	int i;

	// Unlock flash (if not already done)
	flash_mcu_unlock();

	// Clear previous error (if any)
	reg_wr(FLASH_SR, 0xC3FB);

	// Start programing sequence
	v = reg_rd(FLASH_CR);
	v |= 1; // Set PG bit
	reg_wr(FLASH_CR, v);

	while(len)
	{
		// Extract 4 bytes from source buffer (or 0xFF)
		v = data[0]; len--;
		if (len){ v |= (data[1] <<  8); len--; } else v |= 0x0000FF00;
		if (len){ v |= (data[2] << 16); len--; } else v |= 0x00FF0000;
		if (len){ v |= (data[3] << 24); len--; } else v |= 0xFF000000;
		// Write first 32bits word
		*(volatile u32 *)(addr + 0) = v;

		// Extract 4 bytes from source buffer (or 0xFF)
		if (len){ v  = (data[4] <<  0); len--; } else v  = 0x000000FF;
		if (len){ v |= (data[5] <<  8); len--; } else v |= 0x0000FF00;
		if (len){ v |= (data[6] << 16); len--; } else v |= 0x00FF0000;
		if (len){ v |= (data[7] << 24); len--; } else v |= 0xFF000000;
		// Write second 32bits word
		*(volatile u32 *)(addr + 4) = v;

		// Wait end of operation (BSY flag)
		for (i = 0; i < 0x10000000; i++)
		{
			v = reg_rd(FLASH_SR);
			if (v & (3 << 16))
				continue;
			break;
		}
		v = reg_rd(FLASH_SR);
		// Check for errors
		if (v & 0xC3FA)
			goto err;
		// Check EOP, clear it if set
		if (v & (1 << 0))
			reg_wr(FLASH_SR, 1);

		// Update address and prepare for the next write cycle
		addr += 8;
		data += 8;
	}

	// Clear PG bit
	reg_wr(FLASH_CR, 0);
	// Re-enable protection
	flash_mcu_lock();

	return(0);

err:
#ifdef FLASH_MCU_DEBUG
	log_print(LOG_DBG, "Flash: Write error at %32x, SR=%32x\n", addr, v);
#endif
	// Clear (all) errors bits
	reg_wr(FLASH_SR, 0xC3FA);
	// Operation aborted, clear PG
	reg_wr(FLASH_CR, 0);
	// Re-enable protection
	flash_mcu_lock();
	return(-1);
}
/* EOF */
