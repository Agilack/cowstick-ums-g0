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

static int usb_bulk_rx(u8 *data, uint len);
static int usb_bulk_tx(void);

/**
 * @brief Initialize generic BULK module
 *
 * Dummy function used to initialize generic interface
 */
void usb_bulk_init(void)
{
	uart_puts("USB_BULK: Initialized\r\n");
}

/**
 * @brief Enable generic BULK interface
 *
 * This function is called by USB core driver when enumeration is complete and
 * a device configuration has been selected. At this point, it is possible to
 * configure and activate interface endpoints.
 */
void usb_bulk_enable(void)
{
	usb_ep_def ep_def;

	/* Configure RX endpoint */
	ep_def.rx = usb_bulk_rx;
	ep_def.tx_complete = 0;
	usb_ep_configure(2, USB_EP_BULK, &ep_def);
	/* Configure TX endpoint */
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
void usb_bulk_reset(void)
{
	uart_puts("USB_BULK: Reset\r\n");
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
/* EOF */
