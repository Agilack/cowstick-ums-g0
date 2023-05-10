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
#include "log.h"
#include "types.h"
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

	log_puts("USB_BULK: Initialized\n");
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

	log_print(LOG_INF, "BULK: Receive %d\n", len);
	if (len > 16)
		len = 16;
	for (i = len; i > 0; i -= 4)
	{
		v = *(u32 *)data;
		log_print(LOG_INF, " %8x", (v & 0xFF));
		if (i > 1)
			log_print(LOG_INF, " %8x", (v >>  8) & 0xFF);
		if (i > 2)
			log_print(LOG_INF, " %8x", (v >> 16) & 0xFF);
		if (i > 3)
			log_print(LOG_INF, " %8x", (v >> 24) & 0xFF);
		data += 4;
	}
	log_print(LOG_INF, "\n");

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
	log_print(LOG_INF, "USB_BULK: TX complete\n");
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

	log_print(LOG_INF, "USB_BULK: Control request (len=%d)\n", len);

	if (data != 0)
	{
		value = *(volatile u32 *)data;
		log_print(LOG_INF, "Receive DATA phase %8x", value & 0xFF);
		if (len > 1)
			log_print(LOG_INF, " %8x", (v >>  8) & 0xFF);
		if (len > 2)
			log_print(LOG_INF, " %8x", (v >> 16) & 0xFF);
		if (len > 3)
			log_print(LOG_INF, " %8x", (v >> 24) & 0xFF);
		log_print(LOG_INF, "\n");
		return(1);
	}

	log_print(LOG_INF, "bmRequestType=%8x ", req->bmRequestType);
	log_print(LOG_INF, "bRequest=%8x ",      req->bRequest);
	log_print(LOG_INF, "wValue=%16x ",       req->wValue);
	log_print(LOG_INF, "wIndex=%16x ",       req->wIndex);
	log_print(LOG_INF, "wLength=%16x\n",      req->wLength);

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

	log_print(LOG_INF, "USB_BULK: Enabled\n");
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
	log_print(LOG_INF, "USB_BULK: Reset\n");
}
/* EOF */
