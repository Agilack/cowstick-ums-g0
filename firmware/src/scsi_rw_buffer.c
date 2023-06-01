/**
 * @file  scsi_rw_buffer.c
 * @brief Extension of SCSI driver for READ_BUFFER and WRITE_BUFFER commands
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
#include "app.h"
#include "scsi_rw_buffer.h"
#include "libc.h"
#include "log.h"
#include "uart.h"
#ifdef SCSI_USE_RW_BUFFER

typedef struct __attribute__((packed)) read10_req_s {
	unsigned opcode    :  8;
	unsigned mode      :  8;
	unsigned buffer_id :  8;
	unsigned offset    : 24;
	unsigned length    : 24;
	unsigned control   :  8;
} read10_req;
typedef struct __attribute__((packed)) write10_req_s {
	unsigned opcode    :  8;
	unsigned mode      :  8;
	unsigned buffer_id :  8;
	unsigned offset    : 24;
	unsigned params    : 24;
	unsigned control   :  8;
} write10_req;

static u8   scsi_echo[1024];

static int echo_read (scsi_context *ctx, read10_req *req);
static int echo_write(scsi_context *ctx, write10_req *req);
static int mem_desc  (scsi_context *ctx, read10_req *req);
static int mem_read  (scsi_context *ctx, read10_req *req);
static int microcode_write(scsi_context *ctx, write10_req *req);

/**
 * @brief Diagnostic function used by host to read device memory
 *
 * @param lun Pointer to the LUN to use for this request
 * @param ctx Pointer to a context structure for this transaction
 * @return integer Positive value on success, negative value on error
 */
int cmd10_read_buffer(lun *lun, scsi_context *ctx)
{
	read10_req *req;
	int result = -3;

#ifdef SCSI_SANITY_EXTRA
	// Sanity check
	if ((ctx == 0) || (ctx->cb == 0) || (ctx->cb_len != 10))
		goto err;
#endif

	// Test if READ BUFFER is allowed by permission mask
	if ((lun->perm & SCSI_PERM_RDBUFFER) == 0)
		goto err_perm;

	req = (read10_req *)ctx->cb;

	switch(req->mode)
	{
		// Mode Data: read data buffer
		case 2:
			result = mem_read(ctx, req);
			break;
		// Mode Descriptor: read only header of buffer descriptor
		case 3:
			result = mem_desc(ctx, req);
			break;
		// Mode Echo: read echo buffer
		case 0x0A:
			result = echo_read(ctx, req);
			break;
		default:
			log_print(LOG_ERR, "SCSI: READ_BUFFER %{error%}, unknown mode=%8x id=%8x offset=%24x length=%d\n", 1, req->mode, req->buffer_id, hton3(req->offset), hton3(req->length));
			goto err_cdb;
	}
	return(result);

// Permission error: read buffer not allowed
err_perm:
// Unknown request or invalid field in CDB
err_cdb:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);

// Internal error (!)
err:
	ctx->sense->key = 0x04; // Hardware error
	ctx->sense->asc = 0x00; // No additional sense information
	ctx->sense->ascq = 0x00;
	return(-3);
}

/**
 * @brief Diagnostic function used to download firware and modify device memory
 *
 * @param lun Pointer to the LUN to use for this request
 * @param cb  Pointer to the received packet structure
 * @param len Length of the received packet
 * @return integer Positive value on success, negative value on error
 */
int cmd10_write_buffer(lun *lun, scsi_context *ctx)
{
	write10_req *req;
	int result = -3;

	// Sanity check
	if ((ctx == 0) || (ctx->cb == 0) || (ctx->cb_len != 10))
		goto err;

	// Test if WRITE BUFFER is allowed by permission mask
	if ((lun->perm & SCSI_PERM_WRBUFFER) == 0)
		goto err_perm;

	req = (write10_req *)ctx->cb;

	// Data phase on ECHO buffer
	switch(req->mode)
	{
		case 0x0A:
			result = echo_write(ctx, req);
			break;
		case 0x04:
		case 0x05:
			result = microcode_write(ctx, req);
			break;
		default:
			log_print(LOG_DBG, "SCSI: WRITE BUFFER %{error%}: Unknown mode %d\n", 1, req->mode);
			goto err_mode;
	}

	return(result);

err_mode:
// Permission error: write buffer not allowed
err_perm:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);

// Internal error (!)
err:
	ctx->sense->key  = 0x04; // Hardware error
	ctx->sense->asc  = 0x00; // No additional sense information
	ctx->sense->ascq = 0x00;
	return(-3);
}

/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

/**
 * @brief Process a READ_BUFFER on internal echo buffer
 *
 * @param ctx Pointer to a context structure for this transaction
 * @param req Pointer to the request structure
 * @return integer Positive value on success, negative value on error
 */
static int echo_read(scsi_context *ctx, read10_req *req)
{
	uint dlen;
	u32 addr;

	// Extract buffer offset and read size from request
	addr = hton3(req->offset);
	dlen = hton3(req->length);

	if (ctx->flags == 0)
	{
		log_print(LOG_DBG, "SCSI: READ_BUFFER (echo) offset=%16x len=%d\n", addr, dlen);
		if (dlen > 1024)
			goto err_overflow;
	}

	// Update length with already sent data
	dlen -= ctx->flags;
	addr += ctx->flags;
	if (dlen <= 0)
		return(0);
	// If expected data length is still larger than scsi buffer
	if (dlen > SCSI_BUFFER_SZ)
		dlen = SCSI_BUFFER_SZ;

	log_print(LOG_DBG, "SCSI: Read echo buffer, send %d bytes\n", dlen);
	addr = (u32)&scsi_echo + addr;
	memcpy(ctx->io_data, (const void *)addr, (int)dlen);
	ctx->io_len = dlen;
	ctx->flags += dlen;

	return(2);

// Invalid address, offset or data length
err_overflow:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);
}

/**
 * @brief Process a WRITE_BUFFER on internal echo buffer
 *
 * @param ctx Pointer to a context structure for this transaction
 * @param req Pointer to the request structure
 * @return integer Positive value on success, negative value on error
 */
static int echo_write(scsi_context *ctx, write10_req *req)
{
	u32  addr;
	uint dlen;
	uint len;

	// Extract buffer offset and write size from request
	addr = hton3(req->offset);
	dlen = hton3(req->params);

	if (ctx->flags == 0)
	{
		log_print(LOG_DBG, "SCSI: WRITE_BUFFER (echo) offset=%d len=%d\n", addr, dlen);
		if ((addr + dlen) > 1024)
			goto err_overflow;
		ctx->io_len = 0;
		ctx->flags  = 1;
		return(3);
	}

	// Update length with already sent data
	dlen -= (ctx->flags - 1);
	addr += (ctx->flags - 1);

	// Compute maximum write length
	len = 1024 - addr;
	if (ctx->io_len < len)
		len = ctx->io_len;
	log_print(LOG_DBG, "SCSI: Write echo buffer, offset=%16x len=%d\n", addr, len);

	addr = (u32)&scsi_echo + addr;
	memcpy((void *)addr, ctx->io_data, (int)len);

	ctx->flags += ctx->io_len;
	ctx->io_len = 0;
	if (ctx->flags < hton3(req->params))
		return(3);
	return(0);

// Invalid address, offset or data length
err_overflow:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);
}

/**
 * @brief Process a READ_BUFFER on memory descriptor
 *
 * @param ctx Pointer to a context structure for this transaction
 * @param req Pointer to the request structure
 * @return integer Positive value on success, negative value on error
 */
static int mem_desc(scsi_context *ctx, read10_req *req)
{
	struct __attribute__((packed)) rsp_descriptor {
		unsigned offset_boundary :  8;
		unsigned buffer_capacity : 24;
	} *rsp;

	rsp = (struct rsp_descriptor *)ctx->io_data;

	log_print(LOG_DBG, "SCSI: READ_BUFFER get descriptor informations id=%d\n", req->buffer_id);

	rsp->offset_boundary = 2; // Four bytes boundary (2^2)
	switch(req->buffer_id)
	{
		// Flash bank 2
		case 0:
			rsp->buffer_capacity = (64 * 1024);
			break;
		// Flash : application region
		case 1:
			rsp->buffer_capacity = (64 * 1024) - 0x2000;
			break;
		default:
			goto err_buffer_id;
	}
	return(1);

// Invalid buffer id specified
err_buffer_id:
	log_print(LOG_DBG, "SCSI: READ_BUFFER ... invalid buffer id\n");
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);
}

/**
 * @brief Process a READ_BUFFER on raw memory
 *
 * @param ctx Pointer to a context structure for this transaction
 * @param req Pointer to the request structure
 * @return integer Positive value on success, negative value on error
 */
static int mem_read(scsi_context *ctx, read10_req *req)
{
	uint dlen;
	u32 addr;

	if (ctx->flags == 0)
	{
		log_print(LOG_DBG, "SCSI: READ_BUFFER (mem) id=%8x offset=%24x length=%d\n", req->buffer_id, hton3(req->offset), hton3(req->length));
		uart_flush();
	}

	// Determine address to read according to buffer id
	switch(req->buffer_id)
	{
		case  0: addr = 0x08020000; break;
		case  1: addr = 0x08010000; break;
		case 16: addr = 0x20010000; break;
		default:
			goto err_buffer_id;
	}

	addr += hton3(req->offset);
	dlen = hton3(req->length);
	// Update according to already processed data
	addr += ctx->flags;
	if (dlen > ctx->flags)
		dlen -= ctx->flags;
	else
		dlen = 0;
	// If remaining length is null ... nothing more to send
	if (dlen == 0)
		return(0);

	if (dlen > SCSI_BUFFER_SZ)
		dlen = SCSI_BUFFER_SZ;
	memcpy(ctx->io_data, (const void *)addr, (int)dlen);
	ctx->flags += dlen;
	ctx->io_len = dlen;
	return(2);

// Invalid buffer id specified
err_buffer_id:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);
}

/**
 * @brief Process a WRITE_BUFFER on custom application stored in flash
 *
 * @param ctx Pointer to a context structure for this transaction
 * @param req Pointer to the request structure
 * @return integer Positive value on success, negative value on error
 */
static int microcode_write(scsi_context *ctx, write10_req *req)
{
	u32 addr;

	if (ctx->flags == 0)
	{
		log_print(LOG_DBG, "SCSI: Write buffer (microcode) len=%d\n", hton3(req->params));

		// Verify microcode maximum size
		if (hton3(req->params) > 65536)
			goto err_overflow;

		// Stop app before modifying microcode memory
		app_stop();
		// Erase current microcode
		for (addr = 0x08010000; addr < 0x08020000; addr += 2048)
			flash_mcu_erase(addr);

		// TODO cleanup microcode RAM ?

		ctx->flags++;
		ctx->io_len = 0;
		return(3);
	}

	addr = (ctx->flags - 1);
	addr += 0x08010000;
	flash_mcu_write(addr, ctx->io_data, (int)ctx->io_len);
	ctx->flags += ctx->io_len;
	ctx->io_len = 0;
	if (ctx->flags < hton3(req->params))
		return(3);
	return(0);

// Invalid address, offset or data length
err_overflow:
	ctx->sense->key  = 0x05; // ILLEGAL REQUEST
	ctx->sense->asc  = 0x24; // INVALID FIELD IN CDB
	ctx->sense->ascq = 0x00;
	return(-3);
}
#endif
/* EOF */
