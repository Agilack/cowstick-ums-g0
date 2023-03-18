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
#include "types.h"
#include "uart.h"
#include "usb_desc.h"
#include "usb.h"
#include "usb_bulk.h"

#ifdef USB_DEBUG
#define DBG_IT_MAX   100
#define DBG_IRQ      (1 << 8)
#define DBG_EP0_REG  (1 << 9)
static u32  dbg_flags;
static uint it_count;
#endif

uint state;
uint dev_addr = 0;

static usb_ep_def ep_defs[7];

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
	it_count = 0;
#endif
	dev_addr = 0;
	state = USB_ST_POWERED;

	for (i = 0; i < 7; i++)
	{
		ep_defs[i].rx = 0;
		ep_defs[i].tx_complete = 0;
	}

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

	/* Initialize USB upper layers (class and interfaces) */
	usb_bulk_init();

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
	/* Nothing to do yet */
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
	if ((ep == 0) || (ep > 7))
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
	if (len == 0)
		ep_r &= ~(u32)(1 << 7); // Clear VTTX
	ep_r ^= (1 << 4); // STATTX0
	ep_r ^= (1 << 5); // STATTX1 : Valid
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

static inline void ep0_feature_clear(u32 hdr);
static inline void ep0_feature_set(u32 hdr);
static inline void ep0_get_descriptor(void);
static inline void ep0_get_configuration(void);
static inline void ep0_get_interface(void);
static inline void ep0_get_status(void);
static inline void ep0_set_address(u32 hdr);
static inline void ep0_set_configuration(u32 hdr);
static inline void ep0_set_descriptor(void);
static inline void ep0_set_interface(u32 hdr);

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

	/* Compute address of EP descriptor entry into table */
	pma_addr = ((u32)USB_RAM + (ep << 3) + 4);
	/* Read endpoint RX descriptor */
	ep_r = *(u32 *)pma_addr;
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

	if (ep_defs[ep - 1].rx != 0)
		ep_defs[ep - 1].rx(data, len);

	*(u32*)pma_addr = ep_r & ~(u32)(0x3FF << 16);

	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r &= ~(u32)(0x4070);  // Keep bits
	ep_r |=  (u32)(1 << 7);  // Keep VTTX (if set)
	ep_r &= ~(u32)(1 << 15); // Clear VTRX
	ep_r ^=  (u32)(3 << 12); // STATRX0 : Valid
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
	u32 ep_r;

	/* Compute address of EP descriptor entry into table */
	pma_addr = ((u32)USB_RAM + (ep << 3) + 0);
	/* Read endpoint TX descriptor */
	ep_r = *(u32 *)pma_addr;

	if (ep_defs[ep - 1].tx_complete != 0)
		ep_defs[ep - 1].tx_complete();
#ifdef USB_INFO
	else
	{
		uart_puts("USB: Endpoint ");
		uart_putdec(ep);
		uart_puts(" transmit complete\r\n");
	}
#endif
	/* Clear endpoint data length */
	*(u32*)pma_addr = ep_r & ~(u32)(0x3FF << 16);

	ep_r = reg_rd(USB_CHEPxR(ep));
	ep_r &= ~(u32)(0x7070);
	ep_r |=  (u32)(1 << 15);
	ep_r &= ~(u32)(1 << 7);
	reg_wr(USB_CHEPxR(ep), ep_r);
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
 * @param hdr First four bytes (header) of request
 */
static inline void ep0_feature_clear(u32 hdr)
{
#ifdef USB_INFO
	u16 wValue;
	wValue = (u16)(hdr >> 16);

	uart_puts("USB: Clear feature ");
	uart_puthex(wValue, 16);
	uart_puts("\r\n");
#else
	(void)hdr;
#endif
	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
}

/**
 * @brief Decode and process a SET_FEATURE request
 *
 * @param hdr First four bytes (header) of request
 */
static inline void ep0_feature_set(u32 hdr)
{
#ifdef USB_INFO
	u16 wValue;
	wValue = (u16)(hdr >> 16);

	uart_puts("USB: Set feature ");
	uart_puthex(wValue, 16);
	uart_puts("\r\n");
#else
	(void)hdr;
#endif

	/* Send ZLP to ack setup and ask data phase */
	ep0_send(0, 0);
}

/**
 * @brief Decode and process a GET_DESCRIPTOR request
 *
 * This function is called by the Endpoint-0 reception handler (ep0_rx) when a
 * get descriptor request has been received. See ep0_rx() for more details.
 */
static inline void ep0_get_descriptor(void)
{
	u8 *pma = (u8 *)USB_RAM;
	u8   pkt_rx[4];
	u32  v;
	uint len;
	//uint wIndex; // Not used yet
	uint wLength;

	/* Read current EP0 RX buffer address */
	v = (*(volatile u32*)(pma + 0x4) & 0xFFFF);

	/* Copy received packet to a byte array */
	*(u32 *)&pkt_rx[0] = *(volatile u32*)(pma + v);
	v = *(volatile u32*)(pma + v + 4);
	// wIndex  = (v & 0xFFFF); // Not used yet
	wLength = ((v >> 16) & 0xFFFF);

	/* Get: Device Descriptor */
	if (pkt_rx[3] == 0x01)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Device Descriptor\r\n");
#endif
		ep0_send(desc_device, desc_device[0]);
	}
	/* Get: Configuration descriptor */
	else if (pkt_rx[3] == 0x02)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Configuration Descriptor (wLength=");
		uart_putdec(wLength);
		uart_puts(")\r\n");
#endif
		/* Get length of the descriptor */
		len = sizeof(desc_cfg);
		/* If this length is longer than data requested by host */
		if (len > wLength)
			/* Then, return no more than requested data */
			len = wLength;
		ep0_send(desc_cfg, len);
	}
	/* Get: Device Qualifier  */
	else if (pkt_rx[3] == 0x06)
	{
#ifdef USB_INFO
		uart_puts("USB: Get Device Qualifier\r\n");
#endif
		ep0_send(desc_dev_qualifier, desc_dev_qualifier[0]);
	}
	/* Get: String Descriptor */
	else if (pkt_rx[3] == 0x03)
	{
#ifdef USB_INFO
		uart_puts("USB: Get String Descriptor ");
		uart_puthex(pkt_rx[2], 8);
		uart_puts("\r\n");
#endif
		/* String index 0 : Lang */
		if (pkt_rx[2] == 0x00)
			ep0_send(usbdev_str_lang, usbdev_str_lang[0]);
		/* String index 1 : Manufacturer */
		else if (pkt_rx[2] == 0x01)
			ep0_send(usbdev_str_manuf, usbdev_str_manuf[0]);
		/* String index 2 : Product */
		else if (pkt_rx[2] == 0x02)
			ep0_send(usbdev_str_product, usbdev_str_product[0]);
		else
		{
			uart_puts("USB: Unknown String Descriptor index\r\n");
			ep0_stall();
		}
	}
	else
	{
		uart_puts("USB: GET_DESCRIPTOR (unknown) ");
		uart_puthex(pkt_rx[0], 8);
		uart_puthex(pkt_rx[1], 8);
		uart_puthex(pkt_rx[2], 8);
		uart_puthex(pkt_rx[3], 8);
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
 */
static inline void ep0_get_status(void)
{
	unsigned short status;
#ifdef USB_INFO
	uart_puts("EP0: GET_STATUS\r\n");
#endif
	/* Bit 1 : Remote wakeup */
	/* Bit 0 : Self powered  */
	status = 0;

	ep0_send((u8 *)&status, 2);
}

/**
 * @brief Decode and process a SET_ADDRESS transaction
 *
 * @param hdr First four bytes (header) of request
 */
static inline void ep0_set_address(u32 hdr)
{
	dev_addr = ((hdr >> 16) & 0x7F);

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
 * @param hdr First four bytes (header) of request
 */
static inline void ep0_set_configuration(u32 hdr)
{
	u16 wValue;

	wValue = (u16)(hdr >> 16);

	uart_puts("USB: Set Configuration ");
	uart_putdec(wValue);
	uart_puts("\r\n");

	/* At this point, it is possible to enable class driver(s) */
	usb_bulk_enable();

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
 * @param hdr First four bytes (header) of request
 */
static inline void ep0_set_interface(u32 hdr)
{
#ifdef USB_INFO
	u16 wValue;

	wValue = (u16)(hdr >> 16);

	uart_puts("USB: Set Interface ");
	uart_putdec(wValue);
	uart_puts("\r\n");
#else
	(void)hdr;
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

static inline void ep0_rx(void)
{
#ifdef USB_DEBUG
	char buf_in[32];
#endif
	u8 *pma = (u8 *)USB_RAM;
	uint len;
	u32 val;
	u32 ep0r;

	ep0r = *(volatile u32*)(pma + 0x4);
	val = *(volatile u32*)(pma + 0x40);

#ifdef USB_DEBUG
	if (dbg_flags & DBG_EP0_REG)
	{
		uart_puts("EP0_RX: "); uart_puthex(ep0r, 32);
		uart_puts(" > ");      uart_puthex(val, 32);
		uart_puts("\r\n");
	}
#endif
	len = ((ep0r >> 16) & 0x3FF);
	if (len == 0)
		goto rx_end;

	/* GET_STATUS */
	if ((val & 0xFFFF) == 0x0080)
		ep0_get_status();
	/* GET_DESCRIPTOR */
	else if ((val & 0xFFFF) == 0x0680)
		ep0_get_descriptor();
	/* GET_CONFIGURATION */
	else if ((val & 0xFFFF) == 0x0880)
		ep0_get_configuration();
	/* GET_INTERFACE */
	else if ((val & 0xFFFF) == 0x0a80)
		ep0_get_interface();
	/* CLEAR_FEATURE */
	else if ((val & 0xFFFF) == 0x0100)
		ep0_feature_clear(val);
	/* SET_FEATURE */
	else if ((val & 0xFFFF) == 0x0300)
		ep0_feature_set(val);
	/* SET_ADDRESS */
	else if ((val & 0xFFFF) == 0x0500)
		ep0_set_address(val);
	/* SET_DESCRIPTOR */
	else if ((val & 0xFFFF) == 0x0700)
		ep0_set_descriptor();
	/* SET_CONFIGURATION */
	else if ((val & 0xFFFF) == 0x0900)
		ep0_set_configuration(val);
	/* SET_INTERFACE */
	else if ((val & 0xFFFF) == 0x0B00)
		ep0_set_interface(val);
	/* Unknown or not supported request */
	else
	{
#ifdef USB_DEBUG
		// Extract data
		*(u32 *)&buf_in[0]  = *(volatile u32*)(pma + 0x40);
		*(u32 *)&buf_in[4]  = *(volatile u32*)(pma + 0x44);
		*(u32 *)&buf_in[8]  = *(volatile u32*)(pma + 0x48);
		*(u32 *)&buf_in[12] = *(volatile u32*)(pma + 0x4C);
		*(u32 *)&buf_in[16] = *(volatile u32*)(pma + 0x50);
		*(u32 *)&buf_in[20] = *(volatile u32*)(pma + 0x54);
		*(u32 *)&buf_in[24] = *(volatile u32*)(pma + 0x58);
		*(u32 *)&buf_in[28] = *(volatile u32*)(pma + 0x5C);
		// Dump received packet
		uart_dump((u8*)buf_in, 32);
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

void USB_Handler(void)
{
	u32  isr_ack = (1 << 9);
	uint ep, dir;
	u32  v = reg_rd(USB_ISTR);
	u32  ep0r;

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
		/* Reset USB class layers */
		usb_bulk_reset();

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
	reg_wr(USB_ISTR, ~isr_ack);
#ifdef USB_DEBUG
	it_count ++;
	if ((DBG_IT_MAX > 0) && (it_count >= DBG_IT_MAX))
		reg_wr(0xE000E180, (1 << 8)); /* Disable USB irq */
#endif
}
/* EOF */
