/**
 * @file  usb.c
 * @brief This file contains USB driver for STM32G0
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
#include "app.h"
#include "libc.h"
#include "types.h"
#include "uart.h"
#include "usb_desc.h"
#include "usb.h"

#ifdef USB_DEBUG
#define DBG_IRQ      (1 << 8)
#define DBG_EP0_REG  (1 << 9)
#define DBG_EP0_REQ  (1 << 10)
static u32  dbg_flags;
#endif

/* If the number of device interface has not been defined, set as 1 */
#ifndef USB_IF_COUNT
#define USB_IF_COUNT 1
#endif
/* If the number of endpoints has not been defined, set as 1 */
#ifndef USB_EP_COUNT
#define USB_EP_COUNT 1
#endif
/* If the number of strings has not been defined, set as 0 */
#ifndef USB_STR_COUNT
#define USB_STR_COUNT 0
#endif

uint state;
uint dev_addr = 0;
static usb_ctrl_request ep0_req;

static usb_if_drv if_drv[USB_IF_COUNT];
static usb_ep_def ep_defs[USB_EP_COUNT];

static void ep0_config(void);
static void ep0_send(const u8 *data, unsigned int len);
static void ep0_stall(void);
void memcpy_to_pma(u8 *dst, const u8 *src, unsigned int len);

/**
 * @brief Initialize USB device interface
 *
 * Activate and initialize USB port for using it as device interface. This
 * function must be called before using any other usb functions.
 */
void usb_init(void)
{
	int i;

#ifdef USB_DEBUG
	dbg_flags = DBG_IRQ /*| DBG_EP0_REG*/;
#endif
	dev_addr = 0;
	state = USB_ST_POWERED;

	/* Clear interface driver table */
	memset(&if_drv,  0, sizeof(usb_if_drv) * USB_IF_COUNT);
	/* Clear endpoint description table */
	memset(&ep_defs, 0, sizeof(usb_ep_def) * USB_EP_COUNT);

	/* Activate USB */
	reg_set(RCC_APBENR1, (1 << 13));

	// Clear PDWN
	reg_clr(USB_CNTR, 2);
	for (i = 0; i < 0x4000; i++)
		asm volatile("nop");

	/* Force reset */
	reg_wr(USB_CNTR, (1 << 0));
	(void)reg_rd(USB_CNTR);
	/* Release reset */
	reg_wr(USB_CNTR, 0);
	(void)reg_rd(USB_CNTR);

	for (i = 0; i < 0x4000; i++)
		asm volatile("nop");

	/* Enable USB interrupt into NVIC */
	reg_wr(0xE000E100, (1 << 8)); /* USB */

	uart_puts("USB Initialized\r\n");
}

void usb_start(void)
{
	u32 v;

	state = USB_ST_POWERED;

	/* Set device address to 0 */
	reg_wr(USB_DADDR, (1 << 7));
	/* Configure EP0 */
	ep0_config();

	/* Configure interrupts */
	reg_wr(USB_ISTR, 0);
	v = (1 << 10); // RST_DCOM
	v |= (1 << 15); // CTR
	v |= (1 << 13); // ERR
	v |= (1 << 14); // Overrun / Underrun
	//v |= (1 << 11); // SUSP
	reg_wr(USB_CNTR, v);

	/* Activate pull up/down to D+/D- (bus connect) */
	reg_wr(USB_BCDR, (1 << 15));

	uart_puts("USB Started\r\n");
}

/**
 * @brief Process periodic or asynchronous USB stuff
 *
 * Some USB functions need to be periodically processed or refreshed, and some
 * other are asynchronous and their processing can be delayed. This function is
 * here to process this kind of events.
 */
void usb_periodic(void)
{
	int i;

	/* Call interfaces periodic callback (if any) */
	for (i = 0; i < USB_IF_COUNT; i++)
	{
		if (if_drv[i].periodic != 0)
			if_drv[i].periodic();
	}
}

/**
 * @brief Send a packet to a specified endpoint
 *
 * @param ep   Endpoint number (1 -> 7)
 * @param data Pointer to a buffer with data to send (may be null)
 * @param len  Number of byte to send during IN transfer
 */
void usb_send(const u8 ep, const u8 *data, unsigned int len)
{
	u8 *pma = (u8 *)USB_RAM;
	u32 offset;
	u32 ep_r;

	/* Sanity check */
	if (/*(ep == 0) || */(ep > 7))
		return;

	/* Read current EP TX buffer address */
	offset = (*(volatile u32*)(pma + (ep << 3)) & 0xFFFF);

	/* If a pointer to data has been given, copy to PMA */
	if (data)
		memcpy_to_pma(pma + offset, data, len);
	/* else, data should be already available into PMA */
	else
	{ /* Nothing to do */ }

	/* Update EP TX buffer descriptor with data len */
	*(volatile u32*)(pma + (ep << 3)) = (len << 16) | offset;
	/* Update EP for IN transfer */
	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r &= ~(u32)(0x7040);
	ep_r |=  (u32)(1 << 15); // Keep VTRX (1 has no effect)
	ep_r &= ~(u32)(1 << 7);  // Clear VTTX
	ep_r ^=  (u32)(3 << 4);  // STATTX : Valid
	reg_wr(USB_CHEPxR(ep), ep_r);
}

/**
 * @brief Configure an endpoint for communication
 *
 * This function can be called by class or interface layer to configure and
 * activate an endpoint. The direction (in, out or both) is defined by the
 * presence of callbacks function pointers into the "def" argument. Offset of
 * endpoint buffers into pma memory is configured into the usb_desc.h file.
 *
 * @param ep   Endpoint number (1 -> 7)
 * @param type Endpoint type (Bulk, Iso, Interrupt)
 * @param def  Pointer to an endpoint definition (for callbacks)
 */
void usb_ep_configure(u8 ep, u8 type, usb_ep_def *def)
{
	usb_ep_def *ep_def;
	u8 *pma = (u8 *)USB_RAM;
	u32 cur, v;

	/* Sanity check */
	if ((ep == 0) || (ep > 7) || (def == 0))
		return;

	ep_def = &ep_defs[ep - 1];

	ep_def->release = def->release;

	pma += (ep << 3);
	/* Configure TX descriptor for selected endpoint */
	if (def->tx_complete)
		*(u32*)(pma + 0) = (u32)((0 << 16) | ep_offsets[ep][0]);
	else
		*(u32*)(pma + 0) = (u32)0x00000000;
	ep_def->tx_complete = def->tx_complete;
	/* Configure RX descriptor for selected endpoint */
	if (def->rx)
		*(u32*)(pma + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | ep_offsets[ep][1]);
	else
		*(u32*)(pma + 4) = (u32)0x00000000;
	ep_def->rx = def->rx;

	cur = reg_rd(USB_CHEPxR(ep));
	v  = (type << 9); // UTYPE (bulk, iso, int, ...)
	v  = (ep   << 0); // Endpoint Address
	// Configure endpoint RX flags
	if (def->rx)
		v |=  (u32)(3 << 12); // STATRX: Valid (wait for rx)
	else
		v &= ~(u32)(3 << 12); // STATTX: Disabled
	if (cur & (1 << 14))
		v |= (1 << 14);  // Clear DTOGRX
	// Configure endpoint TX flags
	if (def->tx_complete)
		v |=  (u32)(2 << 4); // STATTX: NAK
	else
		v &= ~(u32)(3 << 4); // STATTX: Disabled
	reg_wr(USB_CHEPxR(ep), v);

#ifdef USB_INFO
	uart_puts("USB: Configure EP ");
	uart_puthex(reg_rd(USB_CHEPxR(ep)), 32);
	uart_puts("  TX desc ");
	uart_puthex(*(u32*)(pma + 0), 32);
	uart_puts("  RX desc ");
	uart_puthex(*(u32*)(pma + 4), 32);
	uart_puts("\r\n");
#endif
}

/**
 * @brief Modify (force) the endpoint state
 *
 * This function modify the state of an endpoint for one direction. The new
 * state can be any of the four supported by USB controller (disabled, stall,
 * nak or valid).
 *
 * @param ep    Endpoint number (1 -> 7) and direction (0x80)
 * @param state New state for the endpoint (1=stall, 2=nacked, 3=valid)
 */
void usb_ep_set_state(u8 ep, u8 state)
{
	u32 prev_state;
	u32 ep_r;
	u8  dir;

	/* Extract directio bit */
	dir = (ep & 0x80);
	/* Keep only endpoint number */
	ep  = (ep & 0x7F);

	// Sanity check
	if (ep > 7)
		return;

	state = (state & 3);

	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r |= (u32)(0x8080);  // Keep VTxX (1 has no effect)
	/* Modify the state of TX (IN) direction */
	if (dir)
	{
		/* Get previous state */
		prev_state = ((ep_r >> 4) & 3);
		/* If previous state was STALL, preserve bits but clear DTOG */
		if ((prev_state == USB_EP_STALL) && (ep == 0))
			ep_r &= ~(u32)(0x7000); // Preserve bits
		else
			ep_r &= ~(u32)(0x7040); // Preserve bits
		ep_r ^=  (u32)(state << 4); // STATTX
	}
	/* Enabling a RX (OUT) is done by updating state from NAK to VALID */
	else
	{
		/* Get previous state */
		prev_state = ((ep_r >> 12) & 3);
		/* If previous state was STALL, preserve bits but clear DTOG */
		if ((prev_state == USB_EP_STALL) && (ep == 0))
			ep_r &= ~(u32)(0x0070); // Preserve bits
		else
			ep_r &= ~(u32)(0x4070); // Preserve bits
		ep_r ^=  (u32)(state << 12); // STATRX
	}
	if (state != prev_state)
		reg_wr(USB_CHEPxR(ep), ep_r);
}

/**
 * @brief Register a driver for an interface
 *
 * This function allow to associate a software module to an interface. All
 * upper layers (class drivers) need to receive some bus events (reset, enable)
 * This is done using an interface driver. On startup, all upper layers can
 * call this function to register themself as handler for specified interface.
 *
 * @param num    Identifier of the interface to register
 * @param new_if Pointer to the interface structure to register
 */
int usb_if_register(uint num, usb_if_drv *new_if)
{
#ifdef USB_ASSERT
	// Sanity check
	if ((new_if == 0) || (num >= USB_IF_COUNT))
		return(-1);
#endif

	memcpy(&if_drv[num], new_if, sizeof(usb_if_drv));

	return(0);
}

/**
 * @brief Copy date to USB packet memory array
 *
 * The memory used by USB must be written only with 32bits words. As buffer of
 * data to send are mainly byte arrays into main sram, they must be copied to
 * usb ram. This function can be called to do a copy to usb memory.
 */
void memcpy_to_pma(u8 *dst, const u8 *src, unsigned int len)
{
	int i;
	u32 v;

	for (i = 0; len >= 4; i+=4, len -= 4)
		*(volatile u32*)(dst + i) = *(u32 *)(src + i);

	if (len > 0)
	{
		v = 0;
		if (len > 3)
			v |= (*(src + i + 3) << 24);
		if (len > 2)
			v |= (*(src + i + 2) << 16);
		if (len > 1)
			v |= (*(src + i + 1) <<  8);
		v |= *(src + i);
		*(volatile u32*)(dst + i) = v;
	}
}

/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                          Private  functions                          -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

static inline void ep0_feature_clear(usb_ctrl_request *req);
static inline void ep0_feature_set(usb_ctrl_request *req);
static inline void ep0_get_descriptor(usb_ctrl_request *req);
static inline void ep0_get_configuration(void);
static inline void ep0_get_interface(void);
static inline void ep0_get_status(usb_ctrl_request *req);
static inline void ep0_set_address(usb_ctrl_request *req);
static inline void ep0_set_configuration(usb_ctrl_request *req);
static inline void ep0_set_descriptor(void);
static inline void ep0_set_interface(usb_ctrl_request *req);

/**
 * @brief Process a packet received on endpoint
 *
 * @param ep Endpoint number
 */
static inline void ep_rx(unsigned char ep)
{
	u32 pma_addr;
	u8 *data;
	u32 ep_r;
	u32 len;
	int result;

	/* Compute address of EP descriptor entry into table */
	pma_addr = ((u32)USB_RAM + (ep << 3) + 4);
	/* Read endpoint RX descriptor */
	ep_r = *(volatile u32 *)pma_addr;
	/* Extract received packet parameters */
	len  = ((ep_r >> 16) & 0x3FF);
	data = (u8 *)((u32)USB_RAM + (ep_r & 0xFFFF));

#ifdef USB_INFO
	uart_puts("EP desc ep_desc=");
	uart_puthex(ep_r, 32);
	uart_puts(" len=");
	uart_putdec(len);
	uart_puts(" data=");
	uart_puthex((u32)data, 32);
	uart_puts(" callback ");
	uart_puthex((u32)ep_defs[ep - 1].rx, 32);
	uart_puts("\r\n");
#endif

	result = 1;

	if (ep_defs[ep - 1].rx != 0)
		result = ep_defs[ep - 1].rx(data, len);

	*(volatile u32*)pma_addr = ep_r & ~(u32)(0x3FF << 16);

	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r &= ~(u32)(0x4070);  // Keep bits
	ep_r |=  (u32)(1 << 7);  // Keep VTTX (if set)
	ep_r &= ~(u32)(1 << 15); // Clear VTRX
	if (result)
		ep_r ^=  (u32)(3 << 12); // STATRX : Valid
	else
		ep_r ^=  (u32)(2 << 12); // STATRX : NAK
	reg_wr(USB_CHEPxR(ep), ep_r);
}

/**
 * @brief Process an end-of-transmission event on an endpoint
 *
 * @param ep Endpoint number
 */
static inline void ep_tx(unsigned char ep)
{
	u32 pma_addr;
	u32 ep_r, ep_d;
	int result = 0;

	/* Compute address of EP descriptor entry into table */
	pma_addr = ((u32)USB_RAM + (ep << 3) + 0);
	/* Read endpoint TX descriptor */
	ep_d = *(volatile u32 *)pma_addr;

#ifdef USB_INFO
	uart_puts("USB EP_TX ");
	uart_puthex(ep, 8);
	uart_puts(" ep_desc=");
	uart_puthex(ep_d, 32);
	uart_puts(" callback ");
	uart_puthex((u32)ep_defs[ep - 1].tx_complete, 32);
	uart_puts("\r\n");
#endif

	/* Acknowledge the received buffer (clear VTTX) */
	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r &= ~(u32)(0x7070);
	ep_r |=  (u32)(1 << 15); // Keep VTRX (write 1 has no effect)
	ep_r &= ~(u32)(1 << 7);  // Clear VTTX
	reg_wr(USB_CHEPxR(ep), ep_r);

	if (ep_defs[ep - 1].tx_complete != 0)
		result = ep_defs[ep - 1].tx_complete();
#ifdef USB_INFO
	else
	{
		uart_puts("USB: Endpoint ");
		uart_putdec(ep);
		uart_puts(" transmit complete\r\n");
	}
#endif
	if (result == 0)
	{
		/* Clear endpoint data length */
		*(volatile u32*)pma_addr = ep_d & ~(u32)(0x3FF << 16);
	}
}

/**
 * @brief Configure the control endpoint (EP0)
 *
 * The endpoint0 (EP0) should always be used as control endpoint for bus
 * enumeration and device configuration. This function is used on startup and
 * after a bus reset to prepare and enable EP0.
 */
static void ep0_config(void)
{
	u8 *pma = (u8 *)USB_RAM;
	u32 cur, v;

	/* Configure descriptors for EP0 */
	*(u32*)(pma + 0) = (u32)((0 << 16) | 0x80);
	*(u32*)(pma + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);

	cur = reg_rd(USB_CHEPxR(0));

	v  = (1 << 9);  // UTYPE: Control
	v |= (3 << 12); // STATRX: Valid (wait for rx)
	if (cur & (1 << 14))
		v |= (1 << 14);
	v |= (2 <<  4); // STATTX: NAK
	reg_wr(USB_CHEPxR(0), v);

#ifdef USB_DEBUG
	if (dbg_flags & DBG_EP0_REG)
	{
		uart_puts(" - CHEP0R: ");
		uart_puthex(reg_rd(USB_CHEPxR(0)), 32);
		uart_puts("\r\n");
	}
#endif
}

/**
 * @brief Decode and process a CLEAR_FEATURE request
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_feature_clear(usb_ctrl_request *req)
{
	unsigned char rcpt;

	rcpt = (req->bmRequestType & 0x1F);

#ifdef USB_INFO
	uart_puts("USB: Clear feature ");
	if (rcpt == 0)      uart_puts("DEVICE");
	else if (rcpt == 1) uart_puts("INTERFACE");
	else if (rcpt == 2) uart_puts("ENDPOINT");
	else if (rcpt == 3) uart_puts("'other'");
	else                uart_puts("Unknown/Unsupported");
	uart_puts(" wValue=");
	uart_puthex(req->wValue, 16);
	uart_puts("\r\n");
#endif

	if ((rcpt == 0) && (req->wValue == 1))
	{
		// TODO: Handle DEVICE_REMOTE_WAKEUP
	}
	else if ((rcpt == 0) &&( req->wValue == 2))
	{
		// TODO: Handle TEST_MODE
	}
	/* Handle ENDPOINT_HALT */
	else if ((rcpt == 2) && (req->wValue == 0))
	{
		usb_ep_def *ep_def;
		int result;
		u8  ep, dir;

		/* Extract endpoint id */
		ep  = (u8)(req->wIndex & 0x7F);
		dir = (u8)(req->wIndex & 0x80);
		if (ep <= 7)
		{
			result = 0;
			/* Get access to the EP config */
			ep_def = &ep_defs[ep - 1];
			/* If a feature callback is available, call it */
			if (ep_def->release != 0)
				result = ep_def->release(dir | ep);
			/* If there is no new error, re-enable endpoint */
			if (result == 0)
				usb_ep_set_state(dir | ep, USB_EP_VALID);
			else if (result == 1)
				usb_ep_set_state(dir | ep, USB_EP_NAK);
		}
	}
	else
		goto err;

	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
	return;
err:
	ep0_stall();
}

/**
 * @brief Decode and process a SET_FEATURE request
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_feature_set(usb_ctrl_request *req)
{
	unsigned char rcpt;

	rcpt = (req->bmRequestType & 0x1F);

#ifdef USB_INFO
	uart_puts("USB: Set feature ");
	if (rcpt == 0)      uart_puts("DEVICE");
	else if (rcpt == 1) uart_puts("INTERFACE");
	else if (rcpt == 2) uart_puts("ENDPOINT");
	else if (rcpt == 3) uart_puts("'other'");
	else                uart_puts("Unknown/Unsupported");
	uart_puts(" wValue=");
	uart_puthex(req->wValue, 16);
	uart_puts("\r\n");
#endif

	if ((rcpt == 0) && (req->wValue == 1))
	{
		// TODO: Handle DEVICE_REMOTE_WAKEUP
	}
	else if ((rcpt == 0) &&( req->wValue == 2))
	{
		// TODO: Handle TEST_MODE
	}
	else if ((rcpt == 2) && (req->wValue == 0))
	{
		// TODO: Handle ENDPOINT_HALT
	}
	else
		goto err;

	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
	return;
err:
	ep0_stall();
}

/**
 * @brief Decode and process a GET_DESCRIPTOR request
 *
 * This function is called by the Endpoint-0 reception handler (ep0_rx) when a
 * get descriptor request has been received. See ep0_rx() for more details.
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_get_descriptor(usb_ctrl_request *req)
{
	u8   type, index;
	uint len;

	type = (u8)(req->wValue >> 8);

	/* Get: Device Descriptor */
	if (type == 0x01)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Device Descriptor\r\n");
#endif
		ep0_send(desc_device, desc_device[0]);
	}
	/* Get: Configuration descriptor */
	else if (type == 0x02)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Configuration Descriptor (wLength=");
		uart_putdec(req->wLength);
		uart_puts(")\r\n");
#endif
		/* Get length of the descriptor */
		len = sizeof(desc_cfg);
		/* If this length is longer than data requested by host */
		if (len > req->wLength)
			/* Then, return no more than requested data */
			len = req->wLength;
		ep0_send(desc_cfg, len);
	}
	/* Get: Device Qualifier  */
	else if (type == 0x06)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Device Qualifier\r\n");
#endif
		ep0_send(desc_dev_qualifier, desc_dev_qualifier[0]);
	}
	/* Get: String Descriptor */
	else if (type == 0x03)
	{
		index = (u8)(req->wValue & 0xFF);
#ifdef USB_INFO
		uart_puts("USB: Get String Descriptor ");
		uart_putdec(index);
		uart_puts("\r\n");
#endif
		if (index < USB_STR_COUNT)
		{
			const u8 *desc = usbdev_strings[index];
			ep0_send(desc, desc[0]);
		}
		else
		{
			uart_puts("USB: Unknown String Descriptor index ");
			uart_putdec(index);
			uart_puts("\r\n");
			ep0_stall();
		}
	}
	else
	{
		uart_puts("USB: GET_DESCRIPTOR (unknown)");
		uart_puts(" wValue=");  uart_puthex(req->wValue, 16);
		uart_puts(" wIndex=");  uart_puthex(req->wIndex, 16);
		uart_puts(" wLength="); uart_puthex(req->wLength, 16);
		uart_puts("\r\n");
		ep0_stall();
	}
}

/**
 * @brief Decode and process a GET_CONFIGURATION request
 *
 * This function is called by the Endpoint-0 reception handler (ep0_rx) when
 * a GET_CONFIGURATION request has been received. The configuration currently
 * selected (by a previous SET_CONFIGURATION) is returned.
 */
static inline void ep0_get_configuration(void)
{
	unsigned short selected;
#ifdef USB_INFO
	uart_puts("EP0: GET_CONFIGURATION\r\n");
#endif
	/* TODO: For the moment, we need only one config, so the response
	         is hard-coded to save memory */
	selected = 1;

	ep0_send((u8 *)&selected, 1);
}

/**
 * @brief Decode and process a GET_INTERFACE request
 *
 * This function is called by the Endpoint-0 reception handler (ep0_rx) when
 * a GET_INTERFACE request has been received. Some interface may support
 * multiple alternate settings. This function returns the currently selected
 * setting for one specified interface (9.4.4)
 */
static inline void ep0_get_interface(void)
{
	unsigned short selected;
#ifdef USB_INFO
	uart_puts("EP0: GET_INTERFACE\r\n");
#endif
	selected = 0;

	ep0_send((u8 *)&selected, 1);
}

/**
 * @brief Decode and process a GET_STATUS request
 *
 * This function is called by the Endpoint-0 reception handler (ep0_rx) when
 * host ask device status. Device status content is defined by USB spec with
 * only two option bits (power and wakeup).
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_get_status(usb_ctrl_request *req)
{
	unsigned char  rcpt;
	unsigned short status;

	rcpt = (req->bmRequestType & 0x1F);

#ifdef USB_INFO
	uart_puts("EP0: GET_STATUS for ");
	if (rcpt == 0)
		uart_puts("device");
	else if (rcpt == 1)
	{
		uart_puts("interface ");
		uart_puthex(req->wIndex, 16);
	}
	else if (rcpt == 2)
	{
		uart_puts("endpoint ");
		uart_puthex(req->wIndex, 16);
	}
	else if (rcpt == 3)
		uart_puts("'other'");
	else
	{
		uart_puts("unsupported/reserved ");
		uart_puthex(rcpt, 12);
	}
	uart_puts("\r\n");
#endif

	/* Request is for the device itself */
	if (rcpt == 0)
	{
		/* Bit 1 : Remote wakeup */
		/* Bit 0 : Self powered  */
		status = 0;
	}
	/* Request is for an interface */
	else if (rcpt == 1)
	{
		if (req->wIndex >= USB_IF_COUNT)
			goto err;
		/* According to 9.4.5 interface status is always 0 */
		status = 0;
	}
	/* Request is for an endpoint */
	else if (rcpt == 2)
	{
		if (req->wIndex >= USB_EP_COUNT)
			goto err;
		// TODO Test that specified endpoint is not Halt
		status = 0;
	}
	else
	{
		// TODO Log error ?
		goto err;
	}

	/* Send response */
	ep0_send((u8 *)&status, 2);
	return;
err:
	ep0_stall();
}

/**
 * @brief Decode and process a SET_ADDRESS transaction
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_set_address(usb_ctrl_request *req)
{
	dev_addr = (req->wValue & 0x7F);

#ifdef USB_INFO
	uart_puts("USB: Set address ");
	uart_putdec(dev_addr);
	uart_puts("\r\n");
#endif
	state = USB_ST_ADDRESS;
	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
}

/**
 * @brief Decode and process a SET_CONFIGURATION transaction
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_set_configuration(usb_ctrl_request *req)
{
	int i;

#ifdef USB_INFO
	uart_puts("USB: Set Configuration ");
	uart_putdec(req->wValue);
	uart_puts("\r\n");
#endif

	/* At this point, it is possible to enable interface driver(s) */
	for (i = 0; i < USB_IF_COUNT; i++)
	{
		if (if_drv[i].enable != 0)
			if_drv[i].enable(req->wValue);
	}

	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
}

/**
 * @brief Decode and process a SET_DESCRIPTOR request
 *
 * The SET_DESCRIPTOR request can be used by some device to dynamically update
 * device descriptors or adding new descriptors. This function is not really
 * used here and only respond with "Request Error" (9.4.8).
 */
static inline void ep0_set_descriptor(void)
{
#ifdef USB_INFO
	uart_puts("USB: Set Descriptor (not supported)\r\n");
#endif
	/* As this function is not supported, respond with Request Error */
	ep0_stall();
}

/**
 * @brief Decode and process a SET_INTERFACE request
 *
 * @param req Pointer to a structure with the received packet
 */
static inline void ep0_set_interface(usb_ctrl_request *req)
{
#ifdef USB_INFO
	uart_puts("USB: Set Interface ");
	uart_putdec(req->wValue);
	uart_puts("\r\n");
#else
	(void)req;
#endif
	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
}

/**
 * @brief Send a packet to the control endpoint (EP0)
 *
 * @param data Pointer to a buffer with data to send (may be null)
 * @param len  Number of byte to send during IN transfer
 */
static void ep0_send(const u8 *data, unsigned int len)
{
	u8 *pma = (u8 *)USB_RAM;
	u32 offset;
	u32 ep0r;

	/* Read current EP0 TX buffer address */
	offset = (*(volatile u32*)(pma + 0) & 0xFFFF);

	/* If a pointer to data has been given, copy to PMA */
	if (data)
		memcpy_to_pma(pma + offset, data, len);
	/* else, data should be already available into PMA */
	else
	{ /* Nothing to do */ }

	/* Update EP0 TX buffer descriptor with data len */
	*(volatile u32*)(pma + 0) = (len << 16) | offset;
	/* Update EP0 for IN transfer */
	ep0r = reg_rd(USB_CHEPxR(0));
	ep0r &= ~(u32)(0x7040);
	ep0r |=  (u32)(1 << 15); // Keep VTRX (1 has no effect)
	if (len == 0)
		ep0r &= ~(u32)(1 << 7); // Clear VTTX
	ep0r ^= (1 << 4); // STATTX0
	ep0r ^= (1 << 5); // STATTX1 : Valid
	reg_wr(USB_CHEPxR(0), ep0r);
}

/**
 * @brief Reject current request by responding with STALL
 *
 * Control pipes have the unique ability to return a STALL handshake due to
 * function problems in control transfers (USB 2.0 8.5.3.4) In more general
 * case STALL in used to report an error on endpoint-0 (see 9.2.7).
 */
static void ep0_stall(void)
{
	u8 *pma = (u8 *)USB_RAM;
	u32 offset;
	u32 ep0r;

	/* Read current EP0 TX buffer address */
	offset = (*(volatile u32*)(pma + 0) & 0xFFFF);
	/* Update EP0 TX buffer descriptor with nul data len (0) */
	*(volatile u32*)(pma + 0) = (0 << 16) | offset;
	/* Update EP0 for IN transfer */
	ep0r = reg_rd(USB_CHEPxR(0));
	ep0r &= ~(u32)(0x7040);
	ep0r |=  (u32)(1 << 15); // Keep VTRX (1 has no effect)
	ep0r &= ~(u32)(1 <<  7); // Clear VTTX (no data to transfer)
	ep0r ^=  (u32)(1 <<  4); // STATTX : Stall
	reg_wr(USB_CHEPxR(0), ep0r);
}

/**
 * @brief Process an incoming packet on Endpoint-0 (Control)
 *
 * This function is called by irs handler when a packet has been received on the
 * endpoint-0. The goal of this function is to extract and decode the request
 * and route to dedicated processing functions (see USB2 specs chap 9.3)
 */
static inline void ep0_rx(void)
{
	u8 *pma = (u8 *)USB_RAM;
	u8 *data;
	uint len;
	u32  chep, ep0r;

	chep = reg_rd(USB_CHEPxR(0));
	ep0r = *(volatile u32*)(pma + 0x4);
	len  = ((ep0r >> 16) & 0x3FF);
	data = 0;

#ifdef USB_DEBUG
	if ((dbg_flags & DBG_EP0_REG) || (dbg_flags & DBG_EP0_REQ))
	{
		uart_puts("EP0_RX: CHEP0R="); uart_puthex(chep, 32);
		uart_puts(" CHEP_BD=");       uart_puthex(ep0r, 32);
		uart_puts("\r\n");
	}
#endif

	/* If received a SETUP packet */
	if (chep & (1 << 11))
	{
		if (len < 8)
			goto rx_end;

		/* Copy received data into structure */
		*((u32 *)&ep0_req) = *(volatile u32*)(pma + 0x40);
		*(u32 *)((u8*)&ep0_req + 4) = *(volatile u32*)(pma + 0x44);
	}
	/* In case of a DATA phase with no payload (receive a ZLP) */
	else if (len == 0)
		/* Nothing to do, just ignore it */
		goto rx_end;
	/* Receive a DATA phase, with a payload */
	else
		data = (pma + 0x40);

	/* If received packet is a _standard_ Device-to-Host request */
	if ((ep0_req.bmRequestType & 0xE0) == 0x80)
	{
		switch (ep0_req.bRequest)
		{
			/* GET_STATUS */
			case 0x00:
				ep0_get_status(&ep0_req);
				break;
			/* GET_DESCRIPTOR */
			case 0x06:
				ep0_get_descriptor(&ep0_req);
				break;
			/* GET_CONFIGURATION */
			case 0x08:
				ep0_get_configuration();
				break;
			/* GET_INTERFACE */
			case 0x0a:
				ep0_get_interface();
				break;
			/* Unknown or not supported request */
			default:
				ep0_stall();
		}
	}
	/* If received packet is a _standard_ Host-to-Device request */
	else if ((ep0_req.bmRequestType & 0xE0) == 0)
	{
		switch (ep0_req.bRequest)
		{
			/* CLEAR_FEATURE */
			case 0x01:
				ep0_feature_clear(&ep0_req);
				break;
			/* SET_FEATURE */
			case 0x03:
				ep0_feature_set(&ep0_req);
				break;
			/* SET_ADDRESS */
			case 0x05:
				ep0_set_address(&ep0_req);
				break;
			/* SET_DESCRIPTOR */
			case 0x07:
				ep0_set_descriptor();
				break;
			/* SET_CONFIGURATION */
			case 0x09:
				ep0_set_configuration(&ep0_req);
				break;
			/* SET_INTERFACE */
			case 0x0B:
				ep0_set_interface(&ep0_req);
				break;
			/* Unknown or not supported request */
			default:
				ep0_stall();
		}
	}
	/* If received packet is a class or vendor request for an interface */
	else if ((ep0_req.bmRequestType & 0x1F) == 0x01)
	{
		int result;

		if (ep0_req.wIndex < USB_IF_COUNT)
		{
			if (if_drv[ep0_req.wIndex].ctrl_req != 0)
			{
				result = if_drv[ep0_req.wIndex].ctrl_req(&ep0_req, len, data);
				if (result == 0)
					/* Send ZLP to ack setup and ask data phase */
					ep0_send(0, 0);
				else if (result == 1)
				{
					/* Response already sent by ctrl_req */
				}
				else
					ep0_stall();
			}
			else
				ep0_stall();
		}
		else
			ep0_stall();
	}
	/* Unknown or not supported request */
	else
	{
#ifdef USB_DEBUG
		uart_puts("EP0: Unsupported request (len=");
		uart_putdec(len);
		uart_puts(")\r\n");
		uart_puts("bmRequestType="); uart_puthex(ep0_req.bmRequestType, 8);
		uart_puts(" bRequest=");     uart_puthex(ep0_req.bRequest, 8);
		uart_puts(" wValue=");       uart_puthex(ep0_req.wValue, 16);
		uart_puts(" wIndex=");       uart_puthex(ep0_req.wIndex, 16);
		uart_puts(" wLength=");      uart_puthex(ep0_req.wLength, 16);
		uart_puts("\r\n");
#endif
		ep0_stall();
	}

rx_end:
	*(u32*)(pma + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);
	ep0r = reg_rd(USB_CHEPxR(0));
	ep0r &= ~(u32)(0x4070);  // Keep bits
	ep0r |=  (u32)(1 << 7);  // Keep VTTX (if set)
	ep0r &= ~(u32)(1 << 15); // Clear VTRX
	ep0r ^=  (u32)(3 << 12); // STATRX0 : Valid
	reg_wr(USB_CHEPxR(0), ep0r);
#ifdef USB_DEBUG
	if (dbg_flags & DBG_EP0_REG)
	{
		uart_puts(" - "); uart_puthex(ep0r, 32);
		uart_puts(" > "); uart_puthex(reg_rd(USB_CHEPxR(0)), 32);
		uart_puts("\r\n");
	}
#endif
}

/**
 * @brief Interrupt handler for USB peripheral
 *
 * This function is pointed by the interrupt vector table as the handler for
 * the USB peripheral (see startup.s)
 */
void USB_Handler(void)
{
	u32  isr_ack = (1 << 9);
	uint ep, dir;
	u32  v = reg_rd(USB_ISTR);
	u32  ep0r;
	int  i;

#ifdef USB_DEBUG
	if (dbg_flags & DBG_IRQ)
	{
		uart_puts("USB: IT ");
		uart_puthex(v, 32);
		if (dbg_flags & DBG_EP0_REG)
		{
			uart_puts(" / CHEP0R ");
			uart_puthex(reg_rd(USB_CHEPxR(0)), 32);
		}
		uart_puts("\r\n");
	}
#endif

	/* RST_DCON event */
	if (v & (1 << 10))
	{
		state = USB_ST_DEFAULT;
		/* Reset device address */
		reg_wr(USB_DADDR, (1 << 7));
		ep0_config();
		/* Reset USB class and interfaces layers */
		for (i = 0; i < USB_IF_COUNT; i++)
		{
			if (if_drv[i].reset != 0)
				if_drv[i].reset();
		}
		/* Call custom app reset handler */
		app_reset();

		isr_ack = (1 << 10);
		isr_ack |= ((1 << 11) | (1 << 8));
	}
	/* CTR */
	else if (v & (1 << 15))
	{
		ep  = (v & 0x0F);
		dir = (v & (1 << 4)) ? 1 : 0;

		if (ep)
		{
			if (dir == 1)
				ep_rx((u8)ep);
			else
				ep_tx((u8)ep);
		}
		else
		{
			if (dir == 1)
				ep0_rx();
			else
			{
#ifdef USB_DEBUG
				if (dbg_flags & DBG_EP0_REG)
					uart_puts("EP0: EndTX\r\n");
#endif
				if (state == USB_ST_ADDRESS)
				{
					state = USB_ST_CONFIGURED;
					reg_wr(USB_DADDR, (1 << 7) | dev_addr);
#ifdef USB_INFO
					uart_puts(" - Save new address ");
					uart_putdec(dev_addr);
					uart_puts("\r\n");
#endif
					*(u32*)(USB_DADDR + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);
					ep0r = reg_rd(USB_CHEPxR(0));
					ep0r &= ~(u32)(0x0070);  // Keep bits
					ep0r &= ~(u32)(1 << 7);  // Clear VTTX
					ep0r &= ~(u32)(1 << 15); // Clear VTRX
					ep0r ^=  (u32)(3 << 12); // STATRX : Valid
					reg_wr(USB_CHEPxR(0), ep0r);
				}
				else
				{
					ep0r = reg_rd(USB_CHEPxR(0));
					ep0r &= ~(u32)(0x7070);
					ep0r |=  (u32)(1 << 15);
					ep0r &= ~(u32)(1 << 7);
					reg_wr(USB_CHEPxR(0), ep0r);
#ifdef USB_DEBUG
					if (dbg_flags & DBG_EP0_REG)
					{
						uart_puts(" - "); uart_puthex(ep0r, 32);
						uart_puts(" > "); uart_puthex(reg_rd(USB_CHEPxR(0)), 32);
						uart_puts("\r\n");
					}
#endif
				}
			}
		}
		isr_ack = (1 << 15);
	}
	/* ERR */
	else if (v & (1 << 13))
	{
		uart_puts("USB: Error IT\r\n");
		isr_ack = (1 << 13);
	}
	/* PMA overrun or underrun */
	else if (v & (1 << 14))
	{
		uart_puts("USB: PMA error IT\r\n");
		isr_ack = (1 << 14);
	}
	reg_wr(USB_ISTR, ~isr_ack);
}
/* EOF */
