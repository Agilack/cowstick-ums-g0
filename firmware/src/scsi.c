/**
 * @file  scsi.c
 * @brief This file contains SCSI disk driver
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
#include "scsi.h"
#include "types.h"
#include "uart.h"

static inline int cmd6(u8 *cb, uint len);
static inline int cmd10(u8 *cb, uint len);

static u8   scsi_data[512];
static uint scsi_len;

/**
 * @brief Initialize SCSI disk driver
 *
 */
void scsi_init(void)
{
	uart_puts("SCSI: Initialized\r\n");
}

int scsi_command(u8 *cb, uint len)
{
	int result = -1;
	u8  group;

	// Sanity check
	if ((cb == 0) || (len == 0))
		return(-1);

	group = ((cb[0] >> 5) & 7);

	switch(group)
	{
		// If packet contains a 6-bytes CDB command
		case 0:
			result = cmd6(cb, len);
			break;
		// If packet contains a 10-bytes CBD command
		case 1:
		case 2:
			result = cmd10(cb, len);
			break;
		// If packet contains a 16-bytes CBD command
		case 4:
			uart_puts("SCSI: CBD-16 commands not supported yet\r\n");
			return(-2);
		// If packet contains a 12-bytes CBD command
		case 5:
			uart_puts("SCSI: CBD-12 commands not supported yet\r\n");
			return(-2);
		// If packet contains a 12-bytes CBD command
		case 6:
		case 7:
			uart_puts("SCSI: CBD-Vendor commands not supported yet\r\n");
			return(-2);
		default:
			uart_puts("SCSI: Unknown CBD format\r\n");
			return(-2);
	}

	return(result);
}

const u8 inq[36] = {
	0x00, 0x00, 0x03, 0x02, 32, 0x00, 0x00, 0x00,
	/* T10 Vendor identification */
	'A','G','I','L','A','C','K', ' ',
	/* Product identification */
	'C','o','w','s','t','i','c','k',
	'-','U','M','S',' ',' ',' ',' ',
	/* Product Revision Label */
	'd','e','v','0'
};

u8 *scsi_get_response(uint *len)
{
	if (len)
		*len = scsi_len;

	return(scsi_data);
}

/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* --                            CDB-6 Commands                            -- */
/* -------------------------------------------------------------------------- */

static int cmd6_inquiry(u8 *cb, uint len);
static int cmd6_mode_sense(void);
static int cmd6_test_ready(u8 *cb, uint len);

static inline int cmd6(u8 *cb, uint len)
{
	if (len != 6)
		return(-2);

	switch(cb[0])
	{
		case SCSI_CMD6_TEST_READY:
			return( cmd6_test_ready(cb, len) );
		case SCSI_CMD6_INQUIRY:
			return( cmd6_inquiry(cb, len) );
		case SCSI_CMD6_MODE_SENSE:
			return( cmd6_mode_sense() );
		default:
			uart_puts("SCSI: Unknown CMD6 ");
			uart_puthex(cb[0], 8);
			uart_puts("\r\n");
	}
	return(-1);
}

static inline int cmd6_inquiry(u8 *cb, uint len)
{
	(void)cb;
	(void)len;

	uart_color(2);
	uart_puts("SCSI: Inquiry\r\n");
	uart_color(0);

	memcpy(scsi_data, inq, 36);
	scsi_len = 36;

	return(1);
}

static int cmd6_mode_sense(void)
{
	uart_color(2);
	uart_puts("SCSI: Mode Sense\r\n");
	uart_color(0);
	scsi_data[0] = 0x03;
	scsi_data[1] = 0; // Medium type
	scsi_data[2] = 0; // Specific parameter
	scsi_data[3] = 0; // Block descriptor length
	scsi_len = 4;
	return(1);
}

static int cmd6_test_ready(u8 *cb, uint len)
{
	uart_color(2);
	uart_puts("SCSI: Test Unit Ready\r\n");
	uart_color(0);

	(void)cb;
	(void)len;

	return(0);
}

/* -------------------------------------------------------------------------- */
/* --                           CDB-10  Commands                           -- */
/* -------------------------------------------------------------------------- */

static int cmd10_read(u8 *cb, uint len);
static int cmd10_read_capacity(void);

static inline int cmd10(u8 *cb, uint len)
{
	if (len != 10)
		return(-2);

	switch(cb[0])
	{
		case SCSI_CMD10_READ_CAPACITY:
			return( cmd10_read_capacity() );
		case SCSI_CMD10_READ:
			return( cmd10_read(cb, len) );
	}
	return(-1);
}

static int cmd10_read(u8 *cb, uint len)
{
	u32 lba;
	u16 transfer_length;
	int i;

	(void)len;

	lba = (cb[2] << 24) | (cb[3] << 16) | (cb[4] << 8) | cb[5];
	transfer_length = (cb[7] << 8);
	transfer_length |= (u16)cb[8];

	uart_color(2);
	uart_puts("SCSI: Read block ");
	uart_puthex(lba, 32);
	uart_puts(" count=");
	uart_putdec(transfer_length);
	uart_puts("\r\n");
	uart_color(0);
	uart_flush();

	//memset(scsi_data, 0, 512);
	for (i = 0; i < 512; i++)
		scsi_data[i] = (i & 0xFF);
	scsi_len = 512;

	return(1);
}

static int cmd10_read_capacity(void)
{
	u8 lba[4] = {0x00, 0x00, 0x10, 0x00};
	u8 block_length[4] = {0x00, 0x00, 0x02, 0x00};

	uart_color(2);
	uart_puts("SCSI: Read Capacity\r\n");
	uart_color(0);
	uart_flush();

	memcpy(&scsi_data[0], lba, 4);
	memcpy(&scsi_data[4], block_length, 4);
	scsi_len = 8;

	return(1);
}
/* EOF */
