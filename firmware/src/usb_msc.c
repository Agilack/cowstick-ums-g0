/**
 * @file  usb_msc.c
 * @brief This file contains an USB Mass Storage class driver
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
#include "scsi.h"
#include "types.h"
#include "usb.h"
#include "usb_msc.h"

static void _periodic(void);
static int  usb_if_ctrl(usb_ctrl_request *req, uint len, u8 *data);
static void usb_if_enable(int cfg_id);
static void usb_if_reset(void);
static int usb_ep_release(const u8 ep);
static int usb_ep_rx(u8 *data, uint len);
static int usb_ep_tx(void);

static usb_if_drv msc_if;

static vu32    fsm_state, data_more;
static vu32    rx_flag, tx_flag, err_flag, rst_flag;
static msc_cbw cbw __attribute__((aligned(4)));
static msc_csw csw;

static uint data_len, data_offset;

/**
 * @brief Initialize generic BULK module
 *
 * Dummy function used to initialize generic interface
 */
void usb_msc_init(void)
{
	fsm_state   = MSC_ST_CBW;
	data_more   = 0;
	data_offset = 0;
	rx_flag     = 0;
	tx_flag     = 0;
	err_flag    = 0;
	rst_flag    = 0;

	/* Configure and register USB interface */
	msc_if.periodic = _periodic;
	msc_if.reset    = usb_if_reset;
	msc_if.enable   = usb_if_enable;
	msc_if.ctrl_req = usb_if_ctrl;
	usb_if_register(0, &msc_if);

	log_puts("USB_MSC: Initialized\n");
}


/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

static inline void fsm_cbw(void);
static inline void fsm_csw(void);
static inline void fsm_data_in(void);
static inline void fsm_data_out(void);
static inline void fsm_error(void);

/**
 * @brief Process periodically MSC state machine
 *
 * This function is registered into USB core as periodic handler for the MSC
 * interface. When bus is active (enumeration complete, interface enabled) USB
 * will call this function periodically. MSC use a state machine to process
 * requests : wait CBW, data phase, send CSW.
 */
static void _periodic(void)
{
	/* Process device ResetRecovery request */
	if (rst_flag)
	{
		fsm_state   = MSC_ST_CBW;
		data_more   = 0;
		data_offset = 0;
		rx_flag     = 0;
		tx_flag     = 0;
		err_flag    = 0;
		if (rst_flag == 1)
		{
			rst_flag = 0;
			usb_send(0, 0, 0);
		}
		else
			rst_flag = 0;
		log_print(LOG_INF, "USB_MSC: Reseted\n");
	}

	/* Dispatch to functions dedicated to each state */
	switch(fsm_state)
	{
		case MSC_ST_CBW:
			fsm_cbw();
			break;

		case MSC_ST_DATA_IN:
			fsm_data_in();
			break;

		case MSC_ST_DATA_OUT:
			fsm_data_out();
			break;

		case MSC_ST_CSW:
			fsm_csw();
			break;

		case MSC_ST_ERROR:
			fsm_error();
			break;

		default:
			fsm_state = MSC_ST_CBW;
	}
}

/**
 * @brief Dump content of a received CBW
 *
 * This function print to console the content of the last received CBW. The
 * header fieds are decoded and the command block is only dumped as an array
 * of bytes in hexadecimal.
 */
static void cbw_dump(void)
{
	int i;

	log_print(LOG_DBG, " - Signature:          %32x\n", cbw.signature);
	log_print(LOG_DBG, " - Tag:                %32x\n", cbw.tag);
	log_print(LOG_DBG, " - DataTransferLength: %32x\n", cbw.data_length);
	log_print(LOG_DBG, " - Flags:              %8x\n",  cbw.flags);
	log_print(LOG_DBG, " - LUN:                %8x\n",  cbw.lun);
	log_print(LOG_DBG, " - CBLength:           %8x\n",  cbw.cb_len);
	log_print(LOG_DBG, " - Command Block:\n");
	for (i = 0; i < 16; i++)
		log_print(LOG_DBG, "%8x ", cbw.cb[i]);
	log_print(LOG_DBG, "\n");
}

/**
 * @brief MSC state machine: Wait for a CBW packet to start a new transaction
 *
 * This is the first step of a MSC transaction. The function first wait for a
 * new packet from host then try to decode the CBW inside and call SCSI layer.
 */
static inline void fsm_cbw(void)
{
	int result;

	/* Nothing received -> nothing to do :) */
	if (rx_flag == 0)
		return;
	rx_flag = 0;

#ifdef MSC_DEBUG_CBW
	log_print(LOG_DBG, "USB_MSC: [%{%32x%}] ", LOG_BLU, cbw.tag);
	log_print(LOG_DBG, "Receive CBW data_len=%d\n", cbw.data_length);
#endif

	/* Clear response structure */
	memset(&csw, 0, sizeof(msc_csw));

	result = scsi_command(cbw.cb, cbw.cb_len);
	switch(result)
	{
		/* Success and response available */
		case 0:
			/* If host want a data phase (Hi or Ho) */
			if (cbw.data_length > 0)
			{
				csw.status = 0;
				goto err;
			}

			fsm_state = MSC_ST_CSW;
			break;

		/* Success and IN data phase needed */
		case 1:
		case 2:
		{
			u8  *data;

			/* If host does *not* want data phase (Hn) */
			if (cbw.data_length == 0)
			{
				/* It is a phase error ... */
				csw.status = 0x02;
				/* and residue is ignored in this case */
				csw.residue = 0x00;
				fsm_state = MSC_ST_CSW;
				break;
			}
			/* If host expect to _send_ data (Ho<>Di) */
			else if ((cbw.flags & 0x80) == 0)
			{
				/* It is a phase error ... */
				csw.status = 0x02;
				/* residue should be ignored but update it */
				csw.residue = 0;
				fsm_state = MSC_ST_ERROR;
				usb_ep_set_state(2, USB_EP_STALL);
				break;
			}

			data_offset = 0;
			csw.residue = cbw.data_length;

			data = scsi_get_response(&data_len);
			if (data)
			{
				fsm_state = MSC_ST_DATA_IN;
				if (result == 1)
					data_more = 0;
				else
					data_more = 1;

				/* If host request _less_ data than returned by this command */
				if (data_len > cbw.data_length)
					data_len = cbw.data_length;

				if (data_len > 64)
				{
					data_offset = 64;
					usb_send(1, data, 64);
				}
				else
				{
					data_offset = data_len;
					usb_send(1, data, data_len);
				}
			}
			else
			{
				log_puts("USB_MSC: SCSI error, Data IN but no data\n");
				goto err;
			}
			break;
		}

		/* Success and OUT data phase needed */
		case 3:
		case 4:
		{
			/* If host does *not* want data phase (Hn) */
			if (cbw.data_length == 0)
			{
				/* It is a phase error ... */
				csw.status = 0x02;
				/* residue should be ignored but update it */
				csw.residue = 0;
				fsm_state = MSC_ST_CSW;
				break;
			}
			/* If host expect to _receive_ data (Hi<>Do) */
			else if (cbw.flags & 0x80)
			{
				/* It is a phase error ... */
				csw.status = 0x02;
				/* residue should be ignored but update it */
				csw.residue = 0;
				fsm_state = MSC_ST_ERROR;
				usb_ep_set_state(0x80 | 1, USB_EP_STALL);
				break;
			}

			csw.residue = cbw.data_length;
			data_len    = 0;
			data_offset = 0;
			/* Get expected data length */
			scsi_set_data(0, &data_len);

			/* If host will send _less_ than command expect */
			if (cbw.data_length < data_len)
				data_len = cbw.data_length;

			fsm_state = MSC_ST_DATA_OUT;
			rx_flag = 0;
			usb_ep_set_state(2, USB_EP_VALID);
			break;
		}

		/* Error into SCSI layer, reject this request */
		case -1:
		case -2:
			/* First, dump the content of this request (debug) */
			cbw_dump();
			goto err;
		case -3:
			goto err;
	}
	return;

err:
	/* If the host does not ask for data phase, transition to CSW */
	if (cbw.data_length == 0)
	{
		if (csw.status == 0)
			csw.status = 0x01;
		fsm_state = MSC_ST_CSW;
	}
	/* Host ask for data, STALL the endpoint to report error */
	else
	{
		if (csw.status == 0)
			csw.status = 0x01;
		csw.residue = cbw.data_length;
		fsm_state = MSC_ST_ERROR;
		if (cbw.flags & 0x80)
			usb_ep_set_state(0x80 | 1, USB_EP_STALL);
		else
			usb_ep_set_state(2, USB_EP_STALL);
	}
}

/**
 * @brief MSC state machine: Send the CSW packet to finish current transaction
 *
 * Sending a CSW packet is the last step of an MSC transaction after CBW and
 * data (see fsm_cbw). This packet report to the host a status code for the
 * last transaction (success or error) and an optional length of remaining
 * data (residue). When this packet is sent, the state machine go back to his
 * initial state (CBW) and a new transaction can be started.
 */
static inline void fsm_csw(void)
{
	/* A STALL event was sent, wait for host to clear it */
	if (err_flag == 2)
		return;

	/* If CSW packet has not already been sent */
	if (csw.signature == 0)
	{
#ifdef MSC_DEBUG_CSW
		log_print(LOG_DBG, "USB_MSC: [%{%32x%}] ", LOG_BLU, cbw.tag);
		log_print(LOG_DBG, "Complete (");
		if (csw.status == 0)
			log_print(LOG_DBG, "%{success%}", LOG_GRN);
		else
			log_print(LOG_DBG, "%{error %x%}", LOG_RED, csw.status);
		log_print(LOG_DBG, "), send CSW residue=%d\n", csw.residue);
#endif
		/* Inform SCSI layer that current transaction is complete */
		scsi_complete();
		/* Prepare and send CSW packet */
		csw.signature = 0x53425355;
		csw.tag = cbw.tag;
		usb_send(1, (u8*)&csw, 13);
	}

	/* When CSW has been transmited, wait TX complete */
	if (tx_flag == 1)
	{
		tx_flag  = 0;
		rx_flag  = 0;
		err_flag = 0;
		fsm_state = MSC_ST_CBW;
		/* Re-activate OUT endpoint to receive next request */
		usb_ep_set_state(2, USB_EP_VALID);
	}
}

/**
 * @brief MSC state machine: Data phase device-to-host (IN transaction)
 *
 * This function manage a data phase transfer for the direction device to host
 * (IN). The length of the payload transmited during data phase can have more
 * or less any length. As USB endpoint buffers are small (~64B in HS) the
 * payload is split into intermediate buffers of 512 bytes by SCSI and sent
 * with multiple chunks of 64 bytes.
 */
static void fsm_data_in(void)
{
	int result;

	if (tx_flag == 0)
		return;
	tx_flag = 0;

	/* Update residue with length of data sent */
	if (csw.residue >= data_offset)
		csw.residue -= data_offset;
	else
	{
		// TODO Handle this error !!!!
		csw.residue = 0;
	}

	/* If there is no more data to send, transition to CSW */
	if (data_more == 0)
	{
		fsm_state = MSC_ST_CSW;
		return;
	}

	result = scsi_command(cbw.cb, cbw.cb_len);
	switch(result)
	{
		/* Success and no more data to send */
		case 0:
			fsm_state = MSC_ST_CSW;
			break;
		/* Success and IN data phase needed */
		case 1:
		case 2:
		{
			u8  *data;
			data_offset = 0;
			data = scsi_get_response(&data_len);
			/* If host request _less_ data than returned by this command */
			if (data_len > csw.residue)
				data_len = csw.residue;
			if (data)
			{
				if (result == 1)
					data_more = 0;
				else
					data_more = 1;

				if (data_len > 64)
				{
					data_offset = 64;
					usb_send(1, data, 64);
				}
				else
				{
					data_offset = data_len;
					usb_send(1, data, data_len);
				}
			}
			else
			{
				log_puts("USB_MSC: SCSI error, Data IN early ends\n");
				goto err;
			}
			break;
		}
		default:
			log_puts("USB_MSC: Unknown SCSI result during Data IN\n");
			goto err;
	}
	return;

err:
	fsm_state = MSC_ST_ERROR;
	usb_ep_set_state(0x80 | 1, USB_EP_STALL);
}

/**
 * @brief MSC state machine: Data phase host-to-device (OUT transaction)
 *
 * This function manage a data phase transfer for the direction host to device
 * (OUT). The length of the payload transmited during data phase can have more
 * or less any length. As USB endpoint buffers are small (~64B in HS) the
 * payload is split into intermediate buffers of 512 bytes by SCSI and sent
 * with multiple chunks of 64 bytes.
 */
static void fsm_data_out(void)
{
	int result;

	if (rx_flag == 0)
		return;
	rx_flag = 0;

	/* Update length of processed data */
	csw.residue -= data_offset;

#ifdef MSC_DEBUG_USB
	log_print(LOG_DBG, "USB_MSC: DATA_OUT, %d more bytes to receive\n",
	    csw.residue);
#endif

	result = scsi_command(cbw.cb, cbw.cb_len);
	switch(result)
	{
		/* Success and no more data to send */
		case 0:
			if (csw.residue > 0)
			{
				csw.status = 0;
				fsm_state = MSC_ST_ERROR;
				usb_ep_set_state(2, USB_EP_STALL);
			}
			else
				fsm_state = MSC_ST_CSW;
			break;
		/* Success and OUT data phase needed */
		case 3:
		{
			data_len    = 0;
			data_offset = 0;
			/* Get expected data length */
			scsi_set_data(0, &data_len);

			/* If host will send _less_ than command expect */
			if (csw.residue < data_len)
				data_len = csw.residue;

			fsm_state = MSC_ST_DATA_OUT;
			rx_flag = 0;
			usb_ep_set_state(2, USB_EP_VALID);
			break;
		}
	}
}

/**
 * @brief MSC state machine: Report and clear an error
 *
 * This state is used when something wrong has been detected during data
 * transfer. The endpoint is first marked as STALL to report error (depends
 * on direction), then a CSW packet is sent to finish transaction.
 */
static inline void fsm_error(void)
{
	if (err_flag == 0)
		return;
	/* Clear error flag */
	err_flag = 0;

	fsm_state = MSC_ST_CSW;
}

/**
 * @brief Endpoint is released after a STALL event
 *
 * This function is registered as callback for the release event of both MSC
 * endpoints.
 *
 * @param ep Endpoint id (1 -> 7)
 * @return integer State of the endpoint after release (0=Valid, 1=NAK)
 */
static int usb_ep_release(const u8 ep)
{
#ifdef MSC_DEBUG_USB
	log_print(LOG_DBG, "USB_MSC: Release endpoint %d %d\n", ep, fsm_state);
#else
	(void)ep;
#endif
	if (fsm_state == MSC_ST_ERROR)
		err_flag = 1;
	else if (fsm_state == MSC_ST_CSW)
		err_flag = 1;

	if ((fsm_state == MSC_ST_CBW) && (ep == 2))
		return(0);

	return(1);
}

/**
 * @brief Endpoint OUT event handler
 *
 * This function is registered as callback for the OUT endpoint of the MSC
 * interface (see usb_bulk_enable). When data are received from host, this
 * function is called by USB core driver to process them.
 *
 * @param data Pointer to received data (into PMA memory)
 * @param len  Number of received bytes
 */
static int usb_ep_rx(u8 *data, uint len)
{
	u8  *dout;
	uint avail, i;

#ifdef MSC_DEBUG_USB
	log_print(LOG_DBG, "USB_MSC: Receive %d bytes (fsm=%d)\n",
	    len, fsm_state);
#endif

	if (fsm_state == MSC_ST_DATA_OUT)
	{
		/* First, get available space */
		avail = 0;
		dout = scsi_set_data(0, &avail);
#ifdef MSC_DEBUG_USB
		log_print(LOG_DBG, "USB_MSC: Receive %d bytes, %d available\n",
		    len, avail);
#endif
		if (avail < len)
			len = avail;
		for (i = 0; i < len; i += 4)
		{
			*(vu32 *)dout = *(vu32 *)data;
			data += 4;
			dout += 4;
		}
		scsi_set_data(0, &i);
		data_offset += len;
		if (data_offset >= data_len)
			rx_flag = 1;
		else
			return(1);
	}
	else
	{
		if (len > sizeof(msc_cbw))
		{
			log_puts("USB_MSC: Receive too large packet\n");
			len = sizeof(msc_cbw);
		}

		dout = (u8 *)&cbw;
		for (i = 0; i < len; i += 4)
		{
			*(vu32 *)dout = *(vu32 *)data;
			data += 4;
			dout += 4;
		}
		rx_flag = 1;
	}

	return(0);
}

/**
 * @brief Endpoint IN event handler
 *
 * This function is registered as callback for the IN endpoint of the MSC
 * interface (see usb_if_enable function). To transmit data to host, the
 * function usb_send() is called and data are queued. When the packet has
 * been processed this function is called to notify completion to the
 * interface.
 */
static int usb_ep_tx(void)
{
	uint remains;
	u8   *data;

	if (fsm_state == MSC_ST_DATA_IN)
	{
		if (data_offset == data_len)
			tx_flag = 1;
		else
		{
			remains = (data_len - data_offset);
			data = scsi_get_response(0);
			if (remains > 64)
			{
				usb_send(1, data + data_offset, 64);
				data_offset += 64;
			}
			else
			{
				usb_send(1, data + data_offset, remains);
				data_offset += remains;
			}
			return(1);
		}
	}
	else if (fsm_state == MSC_ST_CSW)
		tx_flag = 1;

	return(0);
}

/**
 * @brief Process a control request sent to the MSC interface
 *
 * @param req  Pointer to a structure with the received control packet
 * @param len  Length of packet
 * @param data Pointer to a buffer with received data (NULL during setup phase)
 */
static int usb_if_ctrl(usb_ctrl_request *req, uint len, u8 *data)
{
	u32 value = 1;

	if (data)
		return(1);

	/* Get Max LUN */
	if ((req->bmRequestType == 0xA1) && (req->bRequest == 0xFE))
	{
		/* Get the number of registered LUN into SCSI driver */
		value = scsi_lun_count();
		/* Value expected is the id of the last lun (count - 1) */
		if (value > 0)
			value --;
		usb_send(0, (u8*)&value, 1);
		log_print(LOG_DBG, "USB_MSC: GetMaxLUN=%d (%d LUN)\n",
		    value, value+1);
	}
	/* Bulk Only class-specific Reset (reset recovery) */
	else if ((req->bmRequestType == 0x21) && (req->bRequest == 0xFF))
	{
		/* To avoid race condition, reset sequence */
		rst_flag = 1;

		log_print(LOG_INF, "USB_MSC: Class RESET\n");
		return(1);
	}
#ifdef MSC_DEBUG_USB
	else
	{
		log_print(LOG_DBG, "USB_MSC: Control request (len=%d)\n", len);

		log_print(LOG_DBG, "bmRequestType=%8x ", req->bmRequestType);
		log_print(LOG_DBG, "bRequest=%8x ",      req->bRequest);
		log_print(LOG_DBG, "wValue=%16x ",       req->wValue);
		log_print(LOG_DBG, "wIndex=%16x ",       req->wIndex);
		log_print(LOG_DBG, "wLength=%16x\n",     req->wLength);
		return(-1);
	}
#else
	(void)len;
#endif
	return(0);
}

/**
 * @brief Enable MSC interface of USB device
 *
 * This function is called by USB core driver when enumeration is complete and
 * a device configuration has been selected. At this point, it is possible to
 * configure and activate interface endpoints.
 */
static void usb_if_enable(int cfg_id)
{
	usb_ep_def ep_def;

	(void)cfg_id;

	/* Configure RX endpoint */
	ep_def.release = usb_ep_release;
	ep_def.rx      = usb_ep_rx;
	ep_def.tx_complete = 0;
	usb_ep_configure(2, USB_EP_BULK, &ep_def);
	/* Configure TX endpoint */
	ep_def.release = usb_ep_release;
	ep_def.rx      = 0;
	ep_def.tx_complete = usb_ep_tx;
	usb_ep_configure(1, USB_EP_BULK, &ep_def);

#ifdef MSC_INFO
	log_print(LOG_DBG, "USB_MSC: Enabled\n");
#endif
}

/**
 * @brief Reset MSC interface
 *
 * This function is called by USB core driver when a bus reset is detected. All
 * data transfer are abort and the interface must wait the next "enable" event
 * to restart communication.
 */
static void usb_if_reset(void)
{
#ifdef MSC_INFO
	log_print(LOG_DBG, "USB_MSC: Reset\n");
#endif
	rst_flag = 2;

	scsi_reset();
}
/* EOF */
