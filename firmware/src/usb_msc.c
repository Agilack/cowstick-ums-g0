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
#include "scsi.h"
#include "types.h"
#include "uart.h"
#include "usb.h"
#include "usb_msc.h"

static void msc_periodic(void);
static int  usb_if_ctrl(usb_ctrl_request *req, uint len, u8 *data);
static void usb_if_enable(int cfg_id);
static void usb_if_reset(void);
static int usb_ep_release(const u8 ep);
static int usb_ep_rx(u8 *data, uint len);
static int usb_ep_tx(void);

static usb_if_drv msc_if;

static vu32    cbw_state;
static msc_cbw cbw;
static msc_csw csw;

static uint data_len, data_offset;

/**
 * @brief Initialize generic BULK module
 *
 * Dummy function used to initialize generic interface
 */
void usb_msc_init(void)
{
	cbw_state = 0;
	data_offset = 0;

	/* Configure and register USB interface */
	msc_if.periodic = msc_periodic;
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

static void msc_periodic(void)
{
	if (cbw_state == 0)
		return;

	if (cbw_state == 1)
	{
		cbw_state = 2;
		uart_puts("USB_MSC: Receive CBW ");
		uart_color(4);
		uart_puthex(cbw.tag, 32);
		uart_color(0);
		uart_puts("\r\n");

		switch(scsi_command(cbw.cb, cbw.cb_len))
		{
			/* Success and response available */
			case 0:
				cbw_state = 3;
				break;
			/* Success and IN data phase needed */
			case 1:
			{
				u8  *data;
				data_offset = 0;
				data = scsi_get_response(&data_len);
				if (data)
				{
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
				break;
			}
			case -1:
			case -2:
				cbw_dump();
				// STALL ?
				cbw_state = 8;
				usb_ep_set_state(0x80 | 1, USB_EP_STALL);
				break;
		}
	}
	else if (cbw_state == 3)
	{
		uart_puts("USB_MSC: Data complete, send CSW ");
		uart_color(4);
		uart_puthex(cbw.tag, 32);
		uart_color(0);
		uart_puts("\r\n");
		uart_flush();
		csw.signature = 0x53425355;
		csw.tag = cbw.tag;
		csw.residue = 0;
		csw.status = 0x00;
		cbw_state = 4;
		usb_send(1, (u8*)&csw, 13);
	}
	else if (cbw_state == 9)
	{
		uart_puts("USB_MSC: Data complete (with error), send CSW ");
		uart_color(4);
		uart_puthex(cbw.tag, 32);
		uart_color(0);
		uart_puts("\r\n");
		uart_flush();
		csw.signature = 0x53425355;
		csw.tag = cbw.tag;
		csw.residue = 0;
		csw.status = 0x02;
		cbw_state = 4;
		usb_send(1, (u8*)&csw, 13);
	}
	else if (cbw_state == 5)
	{
		cbw_state = 0;
		usb_ep_set_state(2, USB_EP_VALID);
	}
}

static int usb_ep_release(const u8 ep)
{
	uart_puts("USB_MSC: Release endpoint ");
	uart_putdec(ep);
	uart_puts(" ");
	uart_putdec(cbw_state);
	uart_puts("\r\n");
	if (cbw_state == 8)
		cbw_state = 9;
	return(0);
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
	uint i;

#ifdef MSC_DEBUG_RX
	uart_puts("USB_MSC: Receive ");
	uart_putdec(len);
	uart_puts(" bytes\r\n");
#endif

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
	cbw_state = 1;

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

	if (cbw_state == 2)
	{
		if (data_offset == data_len)
			cbw_state = 3;
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
	else if (cbw_state == 4)
		cbw_state = 5;
	else if (cbw_state == 8)
		uart_puts("MSC_TX_8\r\n");

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
		value = 0;
		usb_send(0, (u8*)&value, 1);
		uart_puts("USB_MSC: GetMaxLUN = 0 (1 LUN)\r\n");
	}
#ifdef MSC_DEBUG
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
	ep_def.release = 0;
	ep_def.rx = usb_ep_rx;
	ep_def.tx_complete = 0;
	usb_ep_configure(2, USB_EP_BULK, &ep_def);
	/* Configure TX endpoint */
	ep_def.release = usb_ep_release;
	ep_def.rx = 0;
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
