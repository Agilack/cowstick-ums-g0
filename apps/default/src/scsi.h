/**
 * @file  scsi.h
 * @brief Headers and definitions for SCSI module API
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
#ifndef SCSI_H
#define SCSI_H
#include "types.h"

typedef struct lun_s
{
	uint state;
	uint capacity; // Number of 512 bytes sectors
	uint writable;
	uint perm;     // Permission mask
	/* LUN functions */
	int  (*rd)(u32 addr, u32 len, u8 *data);
	int  (*wr)(u32 addr, u32 len, u8 *data);
	int  (*wr_complete)(void);
	int  (*wr_preload)(u32 addr);
	/* LUN vendor extension */
	int  (*cmd_vendor)(struct lun_s *unit, u32 *ctx, u8 *cb, uint len);
} lun;

extern lun *(*scsi_lun_get)(int pos);

#endif
/* EOF */
