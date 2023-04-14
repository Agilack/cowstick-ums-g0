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
#include "scsi.h"
#include "types.h"
#include "uart.h"
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
static vu32    rx_flag, tx_flag, err_flag;
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

	/* Configure and register USB interface */
	msc_if.periodic = _periodic;
	msc_if.reset    = usb_if_reset;
	msc_if.enable   = usb_if_enable;
	msc_if.ctrl_req = usb_if_ctrl;
	usb_if_register(0, &msc_if);

	uart_puts("USB_MSC: Initialized\r\n");
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

	uart_puts(" - Signature:          "); uart_puthex(cbw.signature, 32);   uart_puts("\r\n");
	uart_puts(" - Tag:                "); uart_puthex(cbw.tag, 32);         uart_puts("\r\n");
	uart_puts(" - DataTransferLength: "); uart_puthex(cbw.data_length, 32); uart_puts("\r\n");
	uart_puts(" - Flags:              "); uart_puthex(cbw.flags, 8);        uart_puts("\r\n");
	uart_puts(" - LUN:                "); uart_puthex(cbw.lun, 8);          uart_puts("\r\n");
	uart_puts(" - CBLength:           "); uart_puthex(cbw.cb_len, 8);       uart_puts("\r\n");
	uart_puts(" - Command Block:\r\n");
	for (i = 0; i < 16; i++)
	{
		if (i % 16)
			uart_puts(" ");
		uart_puthex(cbw.cb[i], 8);
	}
	uart_puts("\r\n");
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
	uart_puts("USB_MSC: [");
	uart_color(4);
	uart_puthex(cbw.tag, 32);
	uart_color(0);
	uart_puts("] Receive CBW data_len=");
	uart_putdec(cbw.data_length);
	uart_puts("\r\n");
#endif

	/* Clear response structure */
	memset(&csw, 0, sizeof(msc_csw));

	result = scsi_command(cbw.cb, cbw.cb_len);
	switch(result)
	{
		/* Success and response available */
		case 0:
			fsm_state = MSC_ST_CSW;
			break;

		/* Success and IN data phase needed */
		case 1:
		case 2:
		{
			u8  *data;
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
				uart_puts("USB_MSC: SCSI error, Data IN but no data\r\n");
				goto err;
			}
			break;
		}

		/* Success and OUT data phase needed */
		case 3:
		case 4:
		{
			data_len    = 0;
			data_offset = 0;
			/* Get expected data length */
			scsi_set_data(0, &data_len);

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
			break;
	}
	return;

err:
	/* If the host does not ask for data phase, transition to CSW */
	if (cbw.data_length == 0)
	{
		csw.status = 0x01;
		fsm_state = MSC_ST_CSW;
	}
	/* Host ask for data, STALL the endpoint to report error */
	else
	{
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
	/* If CSW packet has not already been sent */
	if (csw.signature == 0)
	{
#ifdef MSC_DEBUG_CSW
		uart_puts("USB_MSC: [");
		uart_color(4);
		uart_puthex(cbw.tag, 32);
		uart_color(0);
		uart_puts("] Complete (");
		if (csw.status == 0)
		{
			uart_color(2);
			uart_puts("success");
		}
		else
		{
			uart_color(1);
			uart_puts("error ");
			uart_puthex(csw.status, 8);
		}
		uart_color(0);
		uart_puts("), send CSW residue=");
		uart_putdec(csw.residue);
		uart_puts("\r\n");
		uart_flush();
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
				uart_puts("USB_MSC: SCSI error, Data IN early ends\r\n");
				goto err;
			}
			break;
		}
		default:
			uart_puts("USB_MSC: Unknown SCSI result during Data IN\r\n");
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

#ifdef MSC_DEBUG_USB
	uart_puts("USB_MSC: DATA_OUT\r\n");
#endif
	result = scsi_command(cbw.cb, cbw.cb_len);
	switch(result)
	{
		/* Success and no more data to send */
		case 0:
			fsm_state = MSC_ST_CSW;
			break;
		/* Success and OUT data phase needed */
		case 3:
		{
			data_len    = 0;
			data_offset = 0;
			/* Get expected data length */
			scsi_set_data(0, &data_len);

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
	/* If CSW status has not already been set, set as error */
	if (csw.status == 0)
		csw.status = 0x02;

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
	uart_puts("USB_MSC: Release endpoint ");
	uart_putdec(ep);
	uart_puts(" ");
	uart_putdec(fsm_state);
	uart_puts("\r\n");
	uart_flush();
#else
	(void)ep;
#endif
	if (fsm_state == MSC_ST_ERROR)
		err_flag = 1;

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
	uart_puts("USB_MSC: Receive ");
	uart_putdec(len);
	uart_puts(" bytes\r\n");
#endif

	if (fsm_state == MSC_ST_DATA_OUT)
	{
		/* First, get available space */
		avail = 0;
		dout = scsi_set_data(0, &avail);
#ifdef MSC_DEBUG_USB
		uart_puts("USB_MSC: Receive ");
		uart_putdec(len);
		uart_puts(" bytes, ");
		uart_putdec(avail);
		uart_puts(" available\r\n");
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
			uart_puts("USB_MSC: Receive too large packet\r\n");
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

	if ((req->bmRequestType == 0xA1) && (req->bRequest == 0xFE))
	{
		/* Get the number of registered LUN into SCSI driver */
		value = scsi_lun_count();
		/* Value expected is the id of the last lun (count - 1) */
		if (value > 0)
			value --;
		usb_send(0, (u8*)&value, 1);
		uart_puts("USB_MSC: GetMaxLUN = ");
		uart_putdec(value);
		uart_puts(" (");
		uart_putdec(value+1);
		uart_puts(" LUN)\r\n");
	}
#ifdef MSC_DEBUG_USB
	else
	{
		uart_puts("USB_MSC: Control request (len=");
		uart_putdec(len);
		uart_puts(")\r\n");

		uart_puts("bmRequestType="); uart_puthex(req->bmRequestType, 8);
		uart_puts(" bRequest=");     uart_puthex(req->bRequest, 8);
		uart_puts(" wValue=");       uart_puthex(req->wValue, 16);
		uart_puts(" wIndex=");       uart_puthex(req->wIndex, 16);
		uart_puts(" wLength=");      uart_puthex(req->wLength, 16);
		uart_puts("\r\n");
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
	uart_puts("USB_MSC: Enabled\r\n");
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
	uart_puts("USB_MSC: Reset\r\n");
#endif
	scsi_init();
}
/* EOF */
