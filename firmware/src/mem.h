/**
 * @file  mem.h
 * @brief Headers and definitions for external memories abstraction layer
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
#ifndef MEM_H
#define MEM_H
#include "types.h"

#define MEM_NODE_COUNT 3

typedef struct mem_node_s
{
	uint  type;
	void *chip;
	u32   cache_addr;
	u8    cache_buffer[4096];
	uint  speed;
} mem_node;

typedef struct mem_flash_chip_s
{
	u8   vendor;
	u16  device_id;
	uint size;
	uint speed;
	char *name;
} mem_flash_chip;

void mem_init(void);
int  mem_detect(void);
mem_node *mem_get_node(uint nid);
int       mem_erase(uint nid, u32 addr, uint len);
int       mem_read (uint nid, u32 addr, uint len, u8 *buffer);
int       mem_write(uint nid, u32 addr, uint len, u8 *buffer);

#endif
