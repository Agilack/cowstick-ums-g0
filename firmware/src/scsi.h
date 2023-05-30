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

#define SCSI_USE_CACHE
#define SCSI_USE_RW_BUFFER /* Allo READ_BUFFER and WRITE_BUFFER commands */

#define SCSI_CMD6_TEST_READY       0x00
#define SCSI_CMD6_REQUEST_SENSE    0x03
#define SCSI_CMD6_INQUIRY          0x12
#define SCSI_CMD6_MODE_SENSE       0x1A
#define SCSI_CMD6_START_STOP_UNIT  0x1B
#define SCSI_CMD6_PA_MEDIA_REMOVAL 0x1E
#define SCSI_CMD10_READ_FORMAT_CAPACITIES 0x23
#define SCSI_CMD10_READ_CAPACITY 0x25
#define SCSI_CMD10_READ          0x28
#define SCSI_CMD10_WRITE         0x2A
#define SCSI_CMD10_WRITE_BUFFER  0x3B
#define SCSI_CMD10_READ_BUFFER   0x3C

#define SCSI_LOG_ERR        (1 << 0)
#define SCSI_LOG_WRN        (1 << 1)
#define SCSI_LOG_INF        (1 << 2)
#define SCSI_LOG_DBG        (1 << 3)
#define SCSI_LOG_TEST_READY (1 << 4)
#define SCSI_LOG_SENSE      (1 << 5)
#define SCSI_LOG_READ       (1 << 8)
#define SCSI_LOG_WRITE      (1 << 9)
#define SCSI_LOG_CAPACITY   (1 << 12)
#define SCSI_LOG_MEDIUM     (1 << 15)

#define SCSI_PERM_RDBUFFER (1 << 28)
#define SCSI_PERM_WRBUFFER (1 << 29)

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

typedef struct __attribute__((packed))
{
	uint code     :  8; // Response code
	uint          :  8; // Obsolete / reserved
	uint key      :  8; // Sense key
	uint info     : 32; // Information
	uint length   :  8; // Additional sense length
	uint spec_inf : 32; // Command specific information
	uint asc      :  8; // Additional sense code
	uint ascq     :  8; // Addicional sense code qualifier
	uint fruc     :  8; // Field replaceable unit code
	uint spec_key : 24; // Sense specific key
} scsi_request_sense;

void scsi_init(void);
void scsi_reset(void);
int  scsi_command(u8 *cb, uint len);
void scsi_complete(void);
uint scsi_lun_count(void);
lun *scsi_lun_get(int pos);
u8  *scsi_get_response(uint *len);
u8  *scsi_set_data(u8 *data, uint *len);

#endif
/* EOF */
