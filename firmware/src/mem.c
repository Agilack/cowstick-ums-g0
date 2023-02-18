/**
 * @file  mem.c
 * @brief This module is an abstraction layer to access external (spi) memories
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
#include "libc.h"
#include "mem.h"
#include "spi.h"
#include "types.h"
#include "uart.h"

//#define MEM_FLASH_INFO
//#define MEM_FLASH_DEBUG

static mem_node nodes[MEM_NODE_COUNT];

static const mem_flash_chip *flash_detect(uint channel);
static int  flash_erase(uint channel, u32 addr);
static int  flash_read(uint channel, u8 *buffer, u32 addr, uint len);
static int  flash_write(uint channel, u8 *buffer, u32 addr, uint len);
static void flash_write_enable(uint channel);

/**
 * @brief Initialize mem module
 *
 * This function initialize variables and internal state of the mem module. To
 * work properly, this function must be called before using any other function
 * of mem.
 */
void mem_init(void)
{
	int i;

	for (i = 0; i < MEM_NODE_COUNT; i++)
		memset(&nodes[i], 0, sizeof(mem_node));
}

/**
 * @brief Detect memory components
 *
 * This function try to detect memory chips connected to each channel/node. The
 * mem module supports flash (nor/nand) or serial sram ic.
 */
int mem_detect(void)
{
	const mem_flash_chip *fc;
	uint i;

	for (i = 0; i < MEM_NODE_COUNT; i++)
	{
		// Reduce speed during chip detect (1MHz)
		spi_set_speed(i+1, 1);

		fc = flash_detect(i+1);
		if (fc)
		{
			nodes[i].type  = 1; // Flash
			nodes[i].chip  = (void *)fc;
			nodes[i].speed = fc->speed;
			continue;
		}

		// TODO try to detect SRAM chips
	}

	return(0);
}

/**
 * @brief Get access to one memory node
 *
 * @param nid Node ID to access
 * @return Pointer to a memory node structure (or NULL if not available)
 */
mem_node *mem_get_node(uint nid)
{
	// Sanity check
	if (nid >= MEM_NODE_COUNT)
		return(0);

	return( &nodes[nid] );
}

/**
 * @brief Erase a memory area
 *
 * @param nid  Identifier of the memory node to erase
 * @param addr Address of the first byte to erase
 * @param len  Number of bytes to erase
 * @return integer Number of erased bytes
 */
int mem_erase(uint nid, u32 addr, uint len)
{
	mem_node *node;

	// Sanity check
	if (nid >= MEM_NODE_COUNT)
		return(0);

	node = &nodes[nid];

	if (node->type == 0)
		return(0);

	/* Update SPI speed */
	spi_set_speed(nid+1, node->speed);

	/* If the chip connected to this node is Flash */
	if (node->type == 1)
	{
		if ((addr & 0xFFF) == 0)
		{
			flash_erase(nid + 1, addr);
			len = 4096;
		}
		else
		{
			uart_puts("MEM: Fail to erase flash (unaligned address)\r\n");
			len = 0;
		}
	}
	/* If the chip connected to this node is SRAM */
	else if (node->type == 2)
	{
		// TODO
	}
	else
	{
#ifdef MEM_FLASH_DEBUG
		uart_puts("MEM: Failed to read (Invalid node type)\r\n");
#endif
		len = 0;
	}

	/* Return number of really erased bytes */
	return((int)len);
}

/**
 * @brief Read memory
 *
 * @param nid  Identifier of the memory node to read from
 * @param addr Address to read
 * @param len  Number of bytes to read
 * @param buffer Pointer to a buffer to store data (if null, use cache)
 * @return Number of readed bytes
 */
int mem_read(uint nid, u32 addr, uint len, u8 *buffer)
{
	mem_node *node;

	// Sanity check
	if (nid >= MEM_NODE_COUNT)
		return(0);

	node = &nodes[nid];

	if (node->type == 0)
		return(0);

	/* Update SPI speed */
	spi_set_speed(nid+1, node->speed);

	/* If the chip connected to this node is Flash */
	if (node->type == 1)
	{
		if (buffer)
			flash_read(nid + 1, buffer, addr, len);
		else
		{
			u32 addr_end, addr_tmp;
			// Read into internal cache must be 4k aligned
			node->cache_addr = (addr & 0xFFFFF000);
			flash_read(nid + 1, node->cache_buffer, node->cache_addr, 4096);
			// Compute number of readed bytes into requested region
			addr_end = (node->cache_addr + 4096);
			addr_tmp = addr + len;
			if (addr_tmp > addr_end)
				len = (addr_end - addr);
		}
	}
	/* If the chip connected to this node is SRAM */
	else if (node->type == 2)
	{
		// TODO
	}
	else
	{
#ifdef MEM_FLASH_DEBUG
		uart_puts("MEM: Failed to read (Invalid node type)\r\n");
#endif
		len = 0;
	}

	/* Return number of really readed bytes */
	return((int)len);
}

/**
 * @brief Write data to memory
 *
 * @param nid Identifier of the memory node to write to
 * @param addr Address to write
 * @param len  Number of bytes to write
 * @param buffer Pointer to a buffer with data to write (if null, use cache)
 * @return Number of written bytes
 */
int mem_write(uint nid, u32 addr, uint len, u8 *buffer)
{
	mem_node *node;

	// Sanity check
	if (nid >= MEM_NODE_COUNT)
		return(0);

	node = &nodes[nid];

	if (node->type == 0)
		return(0);

	/* Update SPI speed */
	spi_set_speed(nid+1, node->speed);

	/* If the chip connected to this node is Flash */
	if (node->type == 1)
	{
		if (buffer)
		{
			// If specified address is aligned to a sector, erase it first
			if ((addr & 0xFFF) == 0)
				flash_erase(nid + 1, addr);
			flash_write(nid + 1, buffer, addr, len);
		}
		else
		{
			flash_erase(nid + 1, node->cache_addr);
			flash_write(nid + 1, node->cache_buffer, node->cache_addr, 4096);
			len = 4096;
		}
	}
	/* If the chip connected to this node is SRAM */
	else if (node->type == 2)
	{
		// TODO
	}
	else
	{
#ifdef MEM_FLASH_DEBUG
		uart_puts("MEM: Failed to write (Invalid node type)\r\n");
#endif
		len = 0;
	}

	return((int)len);
}

/* -------------------------------------------------------------------------- */
/* --                       Private flash functions                        -- */
/* -------------------------------------------------------------------------- */

#define FLASH_CHIPS_COUNT 2
const mem_flash_chip flash_chips[FLASH_CHIPS_COUNT] = {
	{0xC2, 0x201A, 65536, 166, "MX25L51245G"}, // Macronix 512Mbits NOR
	{0x9D, 0x6018, 16384, 166, "IS25LP128F"},  // ISSI 128Mbits NOR
};

/**
 * @brief Try to detect a flash chip connected to one memory slot
 *
 * @param channel Id of the (spi) channel to detect
 * @return Pointer to the flash chip structure if detected, zero if not detected
 */
static const mem_flash_chip *flash_detect(uint channel)
{
	const mem_flash_chip *chip = 0;
	u8  vendor_id;
	u16 device_id;
	u32 r;
	int i;

	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Read JEDEC-ID command */
	spi_rw(channel, 0x9F);
	/* First data byte : Manufacturer ID */
	vendor_id = spi_rw(channel, 0x00);
	/* Second data byte : Device ID (1) */
	r = spi_rw(channel, 0x00);
	device_id = (u16)(r << 8);
	/* Third data byte : Device ID (2) */
	device_id |= (u16)spi_rw(channel, 0x00);
	/* Disable chip (CS) */
	spi_cs(channel, 0);

	if ((vendor_id == 0) || (vendor_id == 0xFF))
		return(0);

	for (i = 0; i < FLASH_CHIPS_COUNT; i++)
	{
		if (flash_chips[i].vendor != vendor_id)
			continue;
		if (flash_chips[i].device_id != device_id)
			continue;
		chip = &flash_chips[i];
		break;
	}
#ifdef MEM_FLASH_DEBUG
	if (chip == 0)
	{
		uart_puts("Unknown flash chip detected, vid=");
		uart_puthex(vendor_id, 8);
		uart_puts(" device=");
		uart_puthex(device_id, 16);
		uart_puts("\r\n");
	}
#endif

	return(chip);
}

/**
 * @brief Erase one (4k) block
 *
 * @param channel Id of the (spi) channel to access
 * @param addr    Address of the block to erase
 * @return integer Zero is returned on success, other values are errors
 */
static int flash_erase(uint channel, u32 addr)
{
	u8  status;
	int i;
#ifdef MEM_FLASH_INFO
	uart_puts("FLASH: Erase 4k sector");
	uart_puts(" address "); uart_puthex(addr, 24);
	uart_puts("\r\n");
#endif
	flash_write_enable(channel);

	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Send command: Block Erase (4k) */
	spi_rw(channel, 0x20);
	/* Send address */
	spi_rw(channel, (addr >> 16) & 0xFF);
	spi_rw(channel, (addr >>  8) & 0xFF);
	spi_rw(channel, (addr >>  0) & 0xFF);
	/* Disable chip (CS) */
	spi_cs(channel, 0);

	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Send command: Read Status Register */
	spi_rw(channel, 0x05);
	/* Poll on busy cleared or error detected */
	for (i = 0; i < 100000; i++) // 0x10000
	{
		status = spi_rw(channel, 0x00);
		if (status & (1 << 5))
		{
			uart_puts("FLASH: Erase ERROR\r\n");
			break;
		}
		else if ((status & 1) == 0)
			break;
	}
	/* Disable chip (CS) */
	spi_cs(channel, 0);

#ifdef MEM_FLASH_DEBUG
	uart_puts("  - status ");
	uart_puthex(status, 8);
	uart_puts(" (");
	uart_putdec((uint)i);
	uart_puts(")\r\n");
#endif
	// TODO Handle error ...

	return(0);
}

/**
 * @brief Read an array of bytes from flash memory
 *
 * @param buffer Pointer to a buffer for output
 * @param addr   Address of the first byte to read
 * @param len    Number of bytes to read
 */
static int flash_read(uint channel, u8 *buffer, u32 addr, uint len)
{
	u8 *p;
	uint i;

#ifdef MEM_FLASH_INFO
	uart_puts("FLASH: Read ");   uart_putdec(len);
	uart_puts(" bytes from 0x"); uart_puthex(addr, 24);
	uart_puts(" ... ");
#endif
	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Read Data command (low speed) */
	spi_rw(channel, 0x03);
	/* Send address */
	spi_rw(channel, (addr >> 16) & 0xFF);
	spi_rw(channel, (addr >>  8) & 0xFF);
	spi_rw(channel, (addr >>  0) & 0xFF);

	p = buffer;
	for (i = 0; i < len; i++)
		*p++ = spi_rw(channel, 0x00);

	/* Disable chip (CS) */
	spi_cs(channel, 0);

#ifdef MEM_FLASH_INFO
	uart_puts("done.\r\n");
#endif

	return(0);
}

/**
 * @brief Write datas into flash memory
 *
 * @param channel Id of the (spi) channel to access
 * @param buffer  Pointer to a buffer with data to write
 * @param addr    Address of the first byte to write
 * @param len     Number of bytes to write
 */
static int flash_write(uint channel, u8 *buffer, u32 addr, uint len)
{
	u8 status;
	u8 *p;
	uint i;

#ifdef MEM_FLASH_INFO
	uart_puts("FLASH: Write "); uart_putdec(len);
	uart_puts(" bytes to ");    uart_puthex(addr, 24);
	uart_puts("\r\n");
#endif

	p    = buffer;

	while(len)
	{
		if (len > 256)
			i = 256;
		else
			i = len;
#ifdef MEM_FLASH_DEBUG
		uart_puts("FLASH: Write page ("); uart_putdec(i);
		uart_puts(" bytes) to ");         uart_puthex(addr, 24);
		uart_puts("\r\n");
#endif
		flash_write_enable(channel);

		/* Enable selected chip (CS) */
		spi_cs(channel, 1);
		/* Page Program command (low speed) */
		spi_rw(channel, 0x02);
		/* Send address */
		spi_rw(channel, (addr >> 16) & 0xFF);
		spi_rw(channel, (addr >>  8) & 0xFF);
		spi_rw(channel, (addr >>  0) & 0xFF);
		/* Send data to write */
		addr += i;
		len  -= i;
		for (; i > 0; i--)
			/* Write one byte */
			spi_rw(channel, *p++);
		/* Disable chip (CS) */
		spi_cs(channel, 0);

		/* Enable selected chip (CS) */
		spi_cs(channel, 1);
		/* Send command: Read Status Register */
		spi_rw(channel, 0x05);
		/* Poll on busy cleared or error detected */
		for (i = 0; i < 100000; i++) // 10000
		{
			status = spi_rw(channel, 0x00);
			if (status & (1 << 5))
			{
				uart_puts("FLASH: Write ERROR\r\n");
				break;
			}
			else if ((status & 1) == 0)
				break;
		}
		/* Disable chip (CS) */
		spi_cs(channel, 0);
	}

	return(0);
}

/**
 * @brief Set the Write Enable Latch (WEL) of the flash
 *
 * The WEL must be set to 1 before any erase/write command, mainly for
 * security reason. This function send a command to set this bit.
 */
static void flash_write_enable(uint channel)
{
#ifdef MEM_FLASH_DEBUG
	uart_puts("FLASH: Set Write Enable bit");
#endif
	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Write Enable command */
	spi_rw(channel, 0x06);
	/* Disable chip (CS) */
	spi_cs(channel, 0);

#ifdef MEM_FLASH_DEBUG
	/* Enable selected chip (CS) */
	spi_cs(channel, 1);
	/* Send command: Read Status Register */
	spi_rw(channel, 0x05);
	/* Log current status */
	uart_puts(", status=");
	uart_puthex(spi_rw(channel, 0x00), 8);
	uart_puts("\r\n");
	/* Disable chip (CS) */
	spi_cs(channel, 0);
#endif
}
/* EOF */
