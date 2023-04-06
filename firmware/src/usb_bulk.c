/**
 * @file  usb_bulk.c
 * @brief This file contains functions for a generic USB BULK interface
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
#include "types.h"
#include "uart.h"
#include "usb.h"
#include "usb_bulk.h"

static int  if_ctrl(usb_ctrl_request *req, uint len, u8 *data);
static void if_enable(int cfg_id);
static void if_reset(void);
static int usb_bulk_rx(u8 *data, uint len);
static int usb_bulk_tx(void);

static usb_if_drv bulk_if;

/**
 * @brief Initialize generic BULK module
 *
 * Dummy function used to initialize generic interface
 */
void usb_bulk_init(void)
{
	/* Configure and register interface */
	bulk_if.periodic = 0;
	bulk_if.reset    = if_reset;
	bulk_if.enable   = if_enable;
	bulk_if.ctrl_req = if_ctrl;
	usb_if_register(0, &bulk_if);

	uart_puts("USB_BULK: Initialized\r\n");
}


/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

/**
 * @brief Endpoint OUT event handler
 *
 * This function is registered as callback for the OUT endpoint of the BULK
 * interface (see usb_bulk_enable). When data are received from host, this
 * function is called by USB core driver to process them.
 *
 * @param data Pointer to received data (into PMA memory)
 * @param len  Number of received bytes
 */
static int usb_bulk_rx(u8 *data, uint len)
{
	u32  v;
	uint i;
	char msg[] = "Hello World!";

	uart_puts("BULK: Receive ");
	uart_putdec(len);
	uart_puts("\r\n");
	if (len > 16)
		len = 16;
	for (i = len; i > 0; i -= 4)
	{
		v = *(u32 *)data;
		uart_puts(" ");
		uart_puthex(v & 0xFF, 8);
		if (i > 1)
		{
			uart_puts(" ");
			uart_puthex((v >> 8) & 0xFF, 8);
		}
		if (i > 2)
		{
			uart_puts(" ");
			uart_puthex((v >> 16) & 0xFF, 8);
		}
		if (i > 3)
		{
			uart_puts(" ");
			uart_puthex((v >> 24) & 0xFF, 8);
		}
		data += 4;
	}
	uart_puts("\r\n");

	usb_send(1, (u8*)msg, 12);

	return(0);
}

/**
 * @brief Endpoint IN event handler
 *
 * This function is registered as callback for the IN endpoint of the BULK
 * interface (see usb_bulk_enable). To transmit data to host, the function
 * usb_send() is called and data are queued. When the packet has been processed
 * this function is called to inform the interface.
 */
static int usb_bulk_tx(void)
{
	uart_puts("USB_BULK: TX complete\r\n");
	return(0);
}

/**
 * @brief Process a control request sent to the interface
 *
 * @param req Pointer to a structure with the received control packet
 * @param len Length of packet
 */
static int if_ctrl(usb_ctrl_request *req, uint len, u8 *data)
{
	u32 value = 1;

	uart_puts("USB_BULK: Control request (len=");
	uart_putdec(len);
	uart_puts(")\r\n");

	if (data != 0)
	{
		value = *(volatile u32 *)data;
		uart_puts("Receive DATA phase ");
		uart_puthex(value & 0xFF, 8);
		if (len > 1)
		{
			uart_puts(" ");
			uart_puthex((value >> 8) & 0xFF, 8);
		}
		if (len > 2)
		{
			uart_puts(" ");
			uart_puthex((value >> 16) & 0xFF, 8);
		}
		if (len > 3)
		{
			uart_puts(" ");
			uart_puthex((value >> 24) & 0xFF, 8);
		}
		uart_puts("\r\n");
		return(1);
	}

	uart_puts("bmRequestType="); uart_puthex(req->bmRequestType, 8);
	uart_puts(" bRequest=");     uart_puthex(req->bRequest, 8);
	uart_puts(" wValue=");       uart_puthex(req->wValue, 16);
	uart_puts(" wIndex=");       uart_puthex(req->wIndex, 16);
	uart_puts(" wLength=");      uart_puthex(req->wLength, 16);
	uart_puts("\r\n");

	/* If request is Device-to-Host and data length is not nul */
	if ((req->bmRequestType & 0x80) && (req->wLength > 0))
	{
		/* Then, send a data response with dummy payload */
		usb_send(0, (u8*)&value, 1);
		return(1);
	}

	return(0);
}

/**
 * @brief Enable generic BULK interface
 *
 * This function is called by USB core driver when enumeration is complete and
 * a device configuration has been selected. At this point, it is possible to
 * configure and activate interface endpoints.
 */
static void if_enable(int cfg_id)
{
	usb_ep_def ep_def;

	(void)cfg_id;

	/* Configure RX endpoint */
	ep_def.release = 0;
	ep_def.rx = usb_bulk_rx;
	ep_def.tx_complete = 0;
	usb_ep_configure(2, USB_EP_BULK, &ep_def);
	/* Configure TX endpoint */
	ep_def.release = 0;
	ep_def.rx = 0;
	ep_def.tx_complete = usb_bulk_tx;
	usb_ep_configure(1, USB_EP_BULK, &ep_def);

	uart_puts("USB_BULK: Enabled\r\n");
}

/**
 * @brief Reset generic BULK interface
 *
 * This function is called by USB core driver when a bus reset is detected. All
 * data transfer are abort and the interface must wait the next "enable" event
 * to restart communication.
 */
static void if_reset(void)
{
	uart_puts("USB_BULK: Reset\r\n");
}
/* EOF */
