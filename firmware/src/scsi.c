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
#include "log.h"
#include "mem.h"
#include "scsi.h"
#include "scsi_rw_buffer.h"
#include "types.h"

static inline int cmd0_vendor(lun *unit, u8 *cb, uint len);
static inline int cmd6(u8 *cb, uint len);
static inline int cmd10(lun *unit, scsi_context *ctx);

static lun  scsi_lun;
static u8   scsi_data[512];
static uint scsi_len;
static u32  scsi_ctx;
static u32  scsi_log;

static scsi_request_sense request_sense;

/**
 * @brief Initialize SCSI disk driver
 *
 * This function initialize the SCSI driver. To work properly, this function
 * must be call before any other of the module (ideally on startup).
 */
void scsi_init(void)
{
	scsi_log = SCSI_LOG_ERR | SCSI_LOG_SENSE;

	/* Clear LUN */
	memset(&scsi_lun, 0, sizeof(lun));
	// TODO Dev only, default value should not allow buffer r/w
	scsi_lun.perm = SCSI_PERM_RDBUFFER | SCSI_PERM_WRBUFFER;

	scsi_reset();

	log_puts("SCSI: Initialized\n");
}

void scsi_reset(void)
{
	scsi_ctx = 0;

	/* Initialize SENSE */
	memset(&request_sense, 0, sizeof(scsi_request_sense));
	request_sense.code = 0x70;
	request_sense.length = 10;

	log_puts("SCSI: Reset\n");
}

/**
 * @brief Decode and process an SCSI command
 *
 * This function process an SCSI command. Some commands can be fully processed
 * in one single call (small data buffers) and some other need multiple steps.
 * To achieve all cases, this function can be called multiple times with the
 * same command block, an internal "scsi_ctx" variable i used to track states.
 * The end of a command is notified using the function scsi_complete (see below)
 *
 * @param cb  Pointer to an array of bytes with received CDB
 * @param len Number of bytes into the CDB
 * @return integer Result of command processing (positive value for success)
 */
int scsi_command(u8 *cb, uint len)
{
	scsi_context context;
	int result = -1;
	u8  group;

	// Sanity check
	if ((cb == 0) || (len == 0))
		return(-1);

	group = ((cb[0] >> 5) & 7);

	// Initialize transaction context structure
	context.cb = cb;
	context.cb_len  = len;
	context.io_data = scsi_data;
	context.io_len  = scsi_len;
	context.flags   = scsi_ctx;
	context.sense   = &request_sense;

	switch(group)
	{
		// If packet contains a 6-bytes CDB command
		case 0:
			result = cmd6(cb, len);
			break;
		// If packet contains a 10-bytes CBD command
		case 1:
		case 2:
			result = cmd10(&scsi_lun, &context);
			break;
		// If packet contains a 16-bytes CBD command
		case 4:
			log_puts("SCSI: CBD-16 commands not supported yet\n");
			goto err_illegal;
		// If packet contains a 12-bytes CBD command
		case 5:
			log_puts("SCSI: CBD-12 commands not supported yet\n");
			goto err_illegal;
		// If packet contains a vendor specific CBD command
		case 6:
		case 7:
			result = cmd0_vendor(&scsi_lun, cb, len);
			break;
		default:
			log_puts("SCSI: Unknown CBD format\n");
			goto err_illegal;
	}

	return(result);

err_illegal:
	request_sense.key = 0x05; // Illegal Request
	request_sense.asc = 0x20; // Invalid Command
	return(-1);
}

/**
 * @brief Notify end of a command
 *
 * This function should be called by module that use SCSI to notify the end of
 * the last command. This function reset internal context.
 */
void scsi_complete(void)
{
	scsi_ctx = 0;
}

/**
 * @brief Get the number of registered LUNs
 *
 * @return integer Number of available LUNs
 */
uint scsi_lun_count(void)
{
	return(1);
}

/**
 * @brief Get access to one LUN strcture
 *
 * @param pos Identifier for the requested LUN
 * @return lun* Pointer to the selected LUN structure (or NULL for error)
 */
lun *scsi_lun_get(int pos)
{
	lun *result = 0;

	if (pos == 0)
		result = &scsi_lun;

	return(result);
}

/**
 * @brief Get access to the SCSI readed data
 *
 * @param len  Pointer to integer where length of data can be stored
 * @return u8* Pointer to a buffer with readed data
 */
u8 *scsi_get_response(uint *len)
{
	if (len)
		*len = scsi_len;

	return(scsi_data);
}

/**
 * @brief Get access to the SCSI buffer for writing data
 *
 * @param data Pointer not used today, reserved
 * @param len  Pointer to an integer where length of write buffer can be stored
 * @return u8* Pointer to a bufer where data to write can be stored
 */
u8 *scsi_set_data(u8 *data, uint *len)
{
	u8 *d;

	(void)data;

	if (len)
	{
		if (*len > 0)
			scsi_len += *len;
		*len = 512 - scsi_len;
	}
	d = scsi_data + scsi_len;
	return(d);
}

/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

/**
 * @brief Decode and process a VENDOR command
 *
 * This function is called by the scsi_command when the received CDB contains a
 * vendor specific command (group 6 or group 7). There is no vendor command
 * directly available into scsi layer, a lun extension is used.
 *
 * @return integer Result returned by dedicated function (-1 if unsupported)
 */
static inline int cmd0_vendor(lun *unit, u8 *cb, uint len)
{
	int result = -1;

	// Sanity check
	if ((len < 1) || (unit == 0))
		return(-1);

	if (scsi_log & SCSI_LOG_DBG)
	{
		log_print(LOG_INF, "SCSI: %{Vendor debug %8x data_len=%d%}\n",
		    LOG_YLW, cb[0], scsi_len);
	}

	if (unit->cmd_vendor)
		result = unit->cmd_vendor(unit, &scsi_ctx, cb, len);

	return(result);
}

/* -------------------------------------------------------------------------- */
/* --                            CDB-6 Commands                            -- */
/* -------------------------------------------------------------------------- */

static inline int cmd6_inquiry(u8 *cb, uint len);
static inline int cmd6_mode_sense(u8 *cb);
static inline int cmd6_prevent_media_removal(u8 *cb);
static inline int cmd6_request_sense(void);
static inline int cmd6_start_stop_unit(u8 *cb);
static inline int cmd6_test_ready(void);

/**
 * @brief Decode and dispatch a CMD6 command to dedicated functions
 *
 * This function is called by the scsi_command when the received CDB contains a
 * six bytes command (CMD6). It is only a big switch/case to branch to function
 * dedicated to each supported commands.
 *
 * @return integer Result returned by dedicated functions (-1 if unsupported)
 */
static inline int cmd6(u8 *cb, uint len)
{
	if (len < 6)
		goto err_illegal;

	switch(cb[0])
	{
		case SCSI_CMD6_TEST_READY:
			return( cmd6_test_ready() );
		case SCSI_CMD6_REQUEST_SENSE:
			return( cmd6_request_sense() );
		case SCSI_CMD6_INQUIRY:
			return( cmd6_inquiry(cb, len) );
		case SCSI_CMD6_MODE_SENSE:
			return( cmd6_mode_sense(cb) );
		case SCSI_CMD6_START_STOP_UNIT:
			return( cmd6_start_stop_unit(cb) );
		case SCSI_CMD6_PA_MEDIA_REMOVAL:
			return( cmd6_prevent_media_removal(cb) );
		default:
			request_sense.key = 0x05; // Illegal Request
			request_sense.asc = 0x20; // Invalid Command
			log_print(LOG_WRN, "SCSI: Unknown CMD6 %8x\n", cb[0]);
	}
	return(-1);

err_illegal:
	request_sense.key = 0x05; // Illegal Request
	request_sense.asc = 0x20; // Invalid Command
	return(-1);
}

/**
 * @brief INQUIRY command allow client to read informations regarding LUN
 *
 * @param cb  Pointer to a byte array with received command
 * @param len Number of bytes into the command
 */
static inline int cmd6_inquiry(u8 *cb, uint len)
{
	const u8 std[36] = {
		0x00, 0x80, 0x02, 0x02, 32, 0x01, 0x00, 0x00,
		/* T10 Vendor identification */
		'A','G','I','L','A','C','K', ' ',
		/* Product identification */
		'C','o','w','s','t','i','c','k',
		'-','U','M','S',' ',' ',' ',' ',
		/* Product Revision Label */
		'd','e','v','0'
	};
	/* VPD 0x00 : Supported Vital Product Data pages */
	const u8 pg00[] = {0, 0x00, 0x00,  3,  0,0x80,0x83};
	/* VPD 0x80 : Unit Serial Number */
	const u8 pg80[] = {0, 0x80, 0x00, 16,
		'7','0','B','3','D','5','4','C',
		'E','8','0','1','0','0','0','0' };
	/* VPD 0x83 : Device Identification */
	const u8 pg83[] = {0, 0x83, 0x00, 24,
		/* Vendor ID identifier */
		0x02, 0x01, 0x00, 0x08, 'A','G','I','L','A','C','K',0x00,
		/* EUI-64 */
		0x01, 0x02, 0x00, 0x08, 0x70, 0xB3, 0xD5, 0x4C, 0xE8, 0x01, 0x00, 0x00
	};
	(void)len;

	log_print(LOG_INF, "%{SCSI: Inquiry%} %8x %8x %8x%8x\n",
	          LOG_YLW, cb[1], cb[2], cb[3], cb[4]);

	if (cb[1] & 0xFE)
		goto err_invalid_field;
	/* If EVPD bit is set, ask a specific VPD page */
	else if (cb[1] & 1)
	{
		switch(cb[2])
		{
			/* Supported Vital Product Data pages */
			case 0x00:
				memcpy(scsi_data, pg00, 6);
				scsi_len = 6;
				break;
			/* Unit Serial Number */
			case 0x80:
				memcpy(scsi_data, pg80, sizeof(pg80));
				scsi_len = sizeof(pg80);
				break;
			/* Device Identification VPD page */
			case 0x83:
				memcpy(scsi_data, pg83, sizeof(pg83));
				scsi_len = sizeof(pg83);
				break;
			default:
				log_print(LOG_WRN, " - Unknown page %8x\n", cb[2]);
				goto err_invalid_field;
				break;
		}
	}
	/* EVPD=0, return standard inquiry structure */
	else
	{
		memcpy(scsi_data, std, sizeof(std));
		scsi_len = sizeof(std);
	}

	return(1);

err_invalid_field:
	/* Sense key = ILLEGAL REQUEST */
	request_sense.key = 0x05;
	/* Additional sense : INVALID FIELD IN CDB */
	request_sense.asc  = 0x24;
	request_sense.ascq = 0x00;
	return(-1);
}

/**
 * @brief MODE SENSE is used to report parameters
 *
 * @param cb  Pointer to a byte array with received command
 */
static inline int cmd6_mode_sense(u8 *cb)
{
#ifdef SCSI_USE_CACHE
	// Cache page
	const u8 cache_page[] = {0x08, 0x12,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00};
	// Control mode page
	u8 ctrl_page[] = {0x0A, 0x0A,
		0x00, 0x00, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00};

#endif
	if (scsi_log & SCSI_LOG_SENSE)
	{
		log_print(LOG_INF, "%{SCSI: Mode Sense %} %8x %8x %8x %8x\n",
		    LOG_YLW, cb[1], cb[2], cb[3], cb[4]);
	}
#ifdef SCSI_USE_CACHE
	scsi_data[0] = 15;
	scsi_data[1] =  0; // Medium type
	scsi_data[2] =  0; // Specific parameter
	scsi_data[3] =  0; // Block descriptor length
	scsi_len = 4;
	// Cache page
	memcpy(scsi_data + scsi_len, cache_page, sizeof(cache_page));
	scsi_len += sizeof(cache_page);
	// Control mode page
	if (scsi_lun.writable == 0)
	{
		scsi_data[2] |= 0x80;
		ctrl_page[4] |= (1 << 3); // SWP
	}
	else
		ctrl_page[4] &= (u8)~(1 << 3);
	memcpy(scsi_data + scsi_len, ctrl_page, sizeof(ctrl_page));
	scsi_len += sizeof(ctrl_page);
	// Update block length
	scsi_data[0] = (u8)scsi_len - 1;
#else
	scsi_data[0] = 0x03;
	scsi_data[1] = 0; // Medium type
	scsi_data[2] = 0; // Specific parameter
	scsi_data[3] = 0; // Block descriptor length
	scsi_len = 4;
#endif
	return(1);
}

/**
 * @brief This command enable or disable the removal of the medium in the LUN
 *
 * This function handle the PREVENT ALLOW MEDIUM REMOVAL command. The logical
 * unit shall not allow medium removal if any initiator currently has medium
 * removal prevented.
 *
 * @param cb  Pointer to a byte array with received command
 */
static inline int cmd6_prevent_media_removal(u8 *cb)
{
	if (scsi_log & SCSI_LOG_MEDIUM)
	{
		log_print(LOG_INF, "%{SCSI: Prevent/Allow Medium Removal %8x%}\n",
		    LOG_YLW, cb[4]);
	}
	// TODO The value is not used yet ... do something ?

	return(0);
}

/**
 * @brief The REQUEST SENSE command transfer current sense data
 *
 * The SENSE data is a structure that describe an error or exceptional
 * condiction. When an error occur, the client can (should !) use this
 * command to get the sense structure.
 */
static inline int cmd6_request_sense(void)
{
	uint len;

	if (scsi_log & SCSI_LOG_SENSE)
	{
		log_print(LOG_INF, "%{SCSI: Request Sense", LOG_YLW);
		log_print(LOG_INF, " key=%8x",  request_sense.key);
		log_print(LOG_INF, " code=%8x", request_sense.asc);
		log_print(LOG_INF, " qual=%8x", request_sense.ascq);
		log_print(LOG_INF, "%}\n");
	}

	len = sizeof(scsi_request_sense);
	memcpy(scsi_data, &request_sense, (int)len);
	scsi_len = len;

	// After returning SENSE data, clear it
	request_sense.key  = 0x00;
	request_sense.asc  = 0x00;
	request_sense.ascq = 0x00;

	return(1);
}

/**
 * @brief The START/STOP UNIT control power and load or eject of the medium
 *
 * @param cb  Pointer to a byte array with received command
 */
static inline int cmd6_start_stop_unit(u8 *cb)
{
	if (scsi_log & SCSI_LOG_MEDIUM)
	{
		log_print(LOG_INF, "%{SCSI: Start/Stop Unit %8x %8x%}\n",
		    LOG_YLW, cb[3], cb[4]);
	}
	// TODO The value is not used yet ... do something ?

	return(0);
}

/**
 * @brief This command is used to indicate whether the logical unit is ready
 *
 */
static inline int cmd6_test_ready(void)
{
	if (scsi_log & SCSI_LOG_TEST_READY)
		log_print(LOG_INF, "%{SCSI: Test Unit Ready%}\n", LOG_YLW);

	if (scsi_lun.state == 0)
	{
		request_sense.key  = 0x02; // NOT READY
		request_sense.asc  = 0x3A; // MEDIUM NOT PRESENT
		request_sense.ascq = 0x00;
		return(-3);
	}

	return(0);
}

/* -------------------------------------------------------------------------- */
/* --                           CDB-10  Commands                           -- */
/* -------------------------------------------------------------------------- */

static inline int cmd10_read(lun *lun, u8 *cb, uint len);
static inline int cmd10_read_capacity(void);
static inline int cmd10_read_format_capacities(void);
static inline int cmd10_write(lun *lun, u8 *cb, uint len);

/**
 * @brief Decode and dispatch a CMD10 command to dedicated functions
 *
 * This function is called by the scsi_command when the received CDB contains
 * a ten bytes command (CMD10). It is only a big switch/case to branch to
 * function dedicated to each supported commands.
 *
 * @return integer Result returned by dedicated functions (-1 if unsupported)
 */
static inline int cmd10(lun *unit, scsi_context *ctx)
{
	int result;

	(void)unit;

	if ((ctx == 0) || (ctx->cb_len < 10))
		goto err_illegal;

	switch(ctx->cb[0])
	{
		case SCSI_CMD10_READ_FORMAT_CAPACITIES:
			return( cmd10_read_format_capacities() );
		case SCSI_CMD10_READ_CAPACITY:
			return( cmd10_read_capacity() );
		case SCSI_CMD10_READ:
			return( cmd10_read(&scsi_lun, ctx->cb, ctx->cb_len) );
		case SCSI_CMD10_WRITE:
			return( cmd10_write(&scsi_lun, ctx->cb, ctx->cb_len) );
#ifdef SCSI_USE_RW_BUFFER
		case SCSI_CMD10_READ_BUFFER:
			result = cmd10_read_buffer(&scsi_lun, ctx);
			scsi_len = ctx->io_len;
			scsi_ctx = ctx->flags;
			return(result);
		case SCSI_CMD10_WRITE_BUFFER:
			result = cmd10_write_buffer(&scsi_lun, ctx);
			scsi_len = ctx->io_len;
			scsi_ctx = ctx->flags;
			return(result);
#endif
		default:
			request_sense.key = 0x05; // Illegal Request
			request_sense.asc = 0x20; // Invalid Command
			log_print(LOG_WRN, "SCSI: Unknown CMD10 %8x\n", ctx->cb[0]);
	}
	return(-1);

err_illegal:
	request_sense.key = 0x05; // Illegal Request
	request_sense.asc = 0x20; // Invalid Command
	return(-1);
}

/**
 * @brief This command ask device to read data and transfer them
 *
 * @param lun Pointer to the LUN to use for this request
 * @param cb  Pointer to the received packet structure
 * @param len Length of the received packet
 * @return integer Positive value on success, negative value on error
 */
static inline int cmd10_read(lun *lun, u8 *cb, uint len)
{
	u16 transfer_length;
	u32 addr;
	struct __attribute__((packed)) packet {
		u8  opcode;
		u8  flags;
		u32 lba;
		u8  group;
		u16 length;
		u8  control;
	} *pkt;

	// Sanity check
	if ((lun == 0) || (lun->rd == 0))
		goto err_lun;

	pkt = (struct packet *)cb;

	(void)len;

	transfer_length = htons(pkt->length);

	if ((scsi_log & SCSI_LOG_READ) && (scsi_ctx == 0))
	{
		log_print(LOG_INF, "%{SCSI: Read block %32x", LOG_YLW, htonl(pkt->lba));
		log_print(LOG_INF, " count=%d",   htons(pkt->length));
		log_print(LOG_INF, " current=%d", scsi_ctx);
		log_print(LOG_INF, "%}\n");
	}

	addr = (htonl(pkt->lba) + scsi_ctx) * 512;
	scsi_len = (uint)lun->rd(addr, 512, scsi_data);

	scsi_ctx++;
	if (scsi_ctx < transfer_length)
		return(2);
	return(1);

err_lun:
	if (scsi_log & SCSI_LOG_ERR)
		log_print(LOG_ERR, "SCSI: %{Read error, invalid LUN %32x%}\n", LOG_RED, lun->rd);
	request_sense.key = 0x04; // Hardware error
	request_sense.asc = 0x01; // No Index/Logical Block signal
	return(-1);
}



static inline int cmd10_read_capacity(void)
{
	struct __attribute__((packed)) response {
		u32 lba;
		u32 block_length;
	} *rsp;

	if (scsi_log & SCSI_LOG_CAPACITY)
		log_print(LOG_INF, "%{SCSI: Read Capacity%}\n", LOG_YLW);

	rsp = (struct response *)&scsi_data;
	scsi_len = sizeof(struct response);

	rsp->lba          = htonl(scsi_lun.capacity);
	rsp->block_length = htonl(512);

	return(1);
}

static inline int cmd10_read_format_capacities(void)
{
	struct __attribute__((packed)) response {
		uint           : 24; // Reserved
		uint length    :  8;
		// Current/Maximum capacity descriptor
		uint nb_blocks : 32;
		uint type      :  8;
		uint block_len : 24;
	} *rsp;

	if (scsi_log & SCSI_LOG_CAPACITY)
		log_print(LOG_INF, "%{SCSI: Read Format Capacities%}\n", LOG_YLW);

	rsp = (struct response *)&scsi_data;
	scsi_len = sizeof(struct response);

	rsp->length = 8;
	rsp->nb_blocks = htonl(16384);
	rsp->type      = 2;
	rsp->block_len = (htonl(512) & 0xFFFFFF);

	return(1);
}

static inline int cmd10_write(lun *lun, u8 *cb, uint len)
{
	u16 transfer_length;
	u32 addr;
	struct __attribute__((packed)) packet {
		u8  opcode;
		u8  flags;
		u32 lba;
		u8  group;
		u16 length;
		u8  control;
	} *req;

	// Sanity check
	if (lun == 0)
		goto err_lun;

	(void)len;
	req = (struct packet *)cb;
	transfer_length = htons(req->length);

	if (scsi_log & SCSI_LOG_WRITE)
	{
		log_print(LOG_INF, "%{SCSI: Write block %32x", LOG_YLW, htonl(req->lba));
		log_print(LOG_INF, " count=%d", transfer_length);
		log_print(LOG_INF, " current=%d", scsi_ctx);
		log_print(LOG_INF, "%}\n");
	}

	/* Verify if LUN is writable ... or not */
	if (scsi_lun.writable == 0)
	{
		log_print(LOG_WRN, "SCSI: Write protected\n");
		request_sense.key  = 0x07; // Data protect
		request_sense.asc  = 0x27; // Write protected
		return(-3);
	}

	if (scsi_ctx == 0)
	{
		addr = htonl(req->lba) * 512;
		// If a preload function is defined for the LUN, call it
		if (lun->wr_preload)
		{
			if( lun->wr_preload(addr) )
				goto err_preload;
		}
	}
	else if (scsi_ctx > 0)
	{
		addr = (htonl(req->lba) + scsi_ctx - 1) * 512;
		if (scsi_log & SCSI_LOG_WRITE)
			log_print(LOG_INF, "SCSI: Write at %32x\n", addr);
		if (lun->wr)
		{
			if ( lun->wr(addr, 512, scsi_data) )
				goto err_write;
		}
	}
	scsi_len = 0;

	scsi_ctx++;
	if (scsi_ctx <= transfer_length)
		return(3);
	// After last write, if a callback function is defined, call it
	if (lun->wr_complete)
	{
		if ( lun->wr_complete() )
			goto err_write;
	}
	return(0);

err_write:
	if (scsi_log & SCSI_LOG_ERR)
		log_print(LOG_ERR, "SCSI: %{Write error at %32x%}\n", LOG_RED, addr);
	request_sense.key = 0x03; // Medium error
	request_sense.asc = 0x0C; // Write error
	return(-1);

err_preload:
	if (scsi_log & SCSI_LOG_ERR)
		log_print(LOG_ERR, "SCSI: %{Write error, preload rejected%}\n", LOG_RED);
	request_sense.key = 0x03; // Medium error
	request_sense.asc = 0x0C; // Write error
	return(-1);

err_lun:
	if (scsi_log & SCSI_LOG_ERR)
		log_print(LOG_ERR, "SCSI: %{Write error, invalid LUN%}\n", LOG_RED);
	request_sense.key = 0x04; // Hardware error
	request_sense.asc = 0x01; // No Index/Logical Block signal
	return(-1);
}
/* EOF */
