/**
 * @file  scsi.h
 * @brief Headers and definitions for SCSI disk driver
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

#define SCSI_CMD6_TEST_READY 0x00
#define SCSI_CMD6_INQUIRY    0x12
#define SCSI_CMD6_MODE_SENSE 0x1A
#define SCSI_CMD10_READ_CAPACITY 0x25
#define SCSI_CMD10_READ          0x28
#define SCSI_CMD10_WRITE         0x2A

typedef struct
{
	u8   op;
	uint lba;
	u8   opt_len;
	u8   control;
} scsi_cdb_6;

void scsi_init(void);
int  scsi_command(u8 *cb, uint len);
void scsi_complete(void);
u8  *scsi_get_response(uint *len);
u8  *scsi_set_data(u8 *data, uint *len);

#endif
/* EOF */
