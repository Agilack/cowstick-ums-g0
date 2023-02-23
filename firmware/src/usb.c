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
#include "usb.h"

#ifdef USB_DEBUG
#define DBG_IT_MAX   100
#define DBG_IRQ      (1 << 8)
#define DBG_EP0_REG  (1 << 9)
static u32  dbg_flags;
static uint it_count;
#endif

uint state = 0;
uint dev_addr = 0;

void ep0_desc(int num);
void memcpy_to_pma(u8 *dst, const u8 *src, unsigned int len);

/**
 * @brief Initialize USB device interface
 *
 * Activate and initialize USB port for using it as device interface. This
 * function must be called before using any other usb functions.
 */
void usb_init(void)
{
	u8 *pma = (u8 *)USB_RAM;
	u32 v;
	int i;

#ifdef USB_DEBUG
	dbg_flags = DBG_IRQ /*| DBG_EP0_REG*/;
	it_count = 0;
#endif
	dev_addr = 0;
	state = 0;

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

	// Configure descriptors for EP0
	*(u32*)(pma + 0) = (u32)((0 << 16) | 0x80);
	*(u32*)(pma + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);
	// Set EP0 as control
	v  = (1 << 9);  // UTYPE: Control
	v |= (3 << 12); // STATRX: Valid (wait for rx)
	v |= (2 <<  4); // STATTX: NAK
	reg_wr(USB_CHEPxR(0), v);

	/* Enable USB interrupt into NVIC */
	reg_wr(0xE000E100, (1 << 8)); /* USB */

	uart_puts("USB Initialized\r\n");
}

void usb_start(void)
{
	u32 v;

	state = 0;

	/* Set device address to 0 */
	reg_wr(USB_DADDR, (1 << 7));

	// configure interrupts
	reg_wr(USB_ISTR, 0);
	v = (1 << 10); // RST_DCOM
	v |= (1 << 15); // CTR
	v |= (1 << 13); // ERR
	//v |= (1 << 11); // SUSP
	reg_wr(USB_CNTR, v);

	reg_wr(USB_BCDR, (1 << 15));

	uart_puts("USB Started\r\n");
}

void usb_periodic(void)
{
/*	if (flag == 1)
	{
		flag = 2;
		ep0_desc();
	}
*/
}

void ep0_config(uint tx_rdy)
{
	u32 cur, v;

	*(u32*)(USB_RAM + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);

	cur = reg_rd(USB_CHEPxR(0));

	v  = (1 << 9);  // UTYPE: Control
	v ^= (3 << 12); // STATRX: Valid (wait for rx)
	if (cur & (1 << 14))
		v |= (1 << 14);
	if (tx_rdy)
	{
		v |= (3 <<  4); // STATTX: Valid
		if (cur & (1 << 6))
			v |= (1 << 6);
	}
	else
		v |= (2 <<  4); // STATTX: NAK
//	v |= (1 << 14) | (1 << 6);
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

const u8 desc_device[] = {
	18,   0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
	0x08, 0x36, 0x20, 0xC7, 0x01, 0x01, 0x01, 0x02,
	0x00, 0x01 };

const u8 desc_dev_qualifier[] = {
	10,   0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
	0x00, 0x00 };

const u8 desc_cfg[] = {
	/* ---- Configuration Descriptor ----*/
	0x09, 0x02,   32, 0x00, 0x01, 0x01, 0x00, 0x80,
	0xFA,
	/* ---- Interface Descriptor ---- */
	0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,
	0x00,
	/* ---- Endpoint (01, Bulk IN) ---- */
	0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x01,
	/* ---- Endpoint (02, Bulk OUT) ---- */
	0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x01,
	/* Empty descriptor (End Of Descriptors marker) */
	0x00, 0x00
	};
const u8 str_lang[] __attribute__((aligned(4))) = {
	0x04, 0x03, 0x09, 0x04
	};
const u8 str_manuf[] __attribute__((aligned(4))) = {
	16, 0x03,
	'A',0x00, 'g',0x00, 'i',0x00, 'l',0x00,
	'a',0x00, 'c',0x00, 'k',0x00
};
const u8 str_product[] __attribute__((aligned(4))) = {
	26, 0x03,
	'C',0x00, 'o',0x00, 'w',0x00, 's',0x00,
	't',0x00, 'i',0x00, 'c',0x00, 'k',0x00,
	'-',0x00, 'u',0x00, 'm',0x00, 's',0x00
};

void ep0_get_string(void)
{
	u8 pkt_rx[8];
	u8 *pma = (u8 *)USB_RAM;
	u32 ep0r;

	/* Copy received packet to a byte array */
	*(u32 *)&pkt_rx[0] = *(volatile u32*)(pma + 0x40);
	*(u32 *)&pkt_rx[4] = *(volatile u32*)(pma + 0x44);

	/* String Descriptor */
	if (pkt_rx[3] == 0x03)
	{
		/* String index 0 : Lang */
		if (pkt_rx[2] == 0x00)
		{
			memcpy_to_pma(pma + 0x80, str_lang, 4);
			/* Update EP0 */
			*(volatile u32*)(pma + 0) = (4 << 16) | 0x80;
		}
		/* String index 1 : Manufacturer */
		else if (pkt_rx[2] == 0x01)
		{
			memcpy_to_pma(pma + 0x80, str_manuf, 16);
			/* Update EP0 */
			*(volatile u32*)(pma + 0) = (16 << 16) | 0x80;
		}
		/* String index 2 : Product */
		else if (pkt_rx[2] == 0x02)
		{
			memcpy_to_pma(pma + 0x80, str_product, 26);
			/* Update EP0 */
			*(volatile u32*)(pma + 0) = (26 << 16) | 0x80;
		}
		else
			return;
		/* Update EP0 for IN transfer */
		ep0r = reg_rd(USB_CHEPxR(0));
		ep0r &= ~(u32)(0x7040);
		ep0r |=  (u32)(1 << 15);
		ep0r ^= (1 << 4);
		ep0r ^= (1 << 5);
		reg_wr(USB_CHEPxR(0), ep0r);
		//uart_puts("USB: Get String Descriptor\r\n");
	}
}

void ep0_desc(int num)
{
	u32 ep0r;
	u32 pma = USB_RAM;
#ifdef USB_DEBUG
	u32 readback;
#endif

	if (num == 1)
	{
		/* Copy payload */
		*(volatile u32*)(pma + 0x80) = *(u32 *)&desc_device[0];
		*(volatile u32*)(pma + 0x84) = *(u32 *)&desc_device[4];
		*(volatile u32*)(pma + 0x88) = *(u32 *)&desc_device[8];
		*(volatile u32*)(pma + 0x8C) = *(u32 *)&desc_device[12];
		*(volatile u32*)(pma + 0x90) = *(u32 *)&desc_device[16];
		/* Update EP0 */
		*(volatile u32*)(pma + 0) = (18 << 16) | 0x80;
	}
	else if (num == 2)
	{
		u32 vext = *(volatile u32*)(pma + 0x44);
		unsigned int wLength = (vext >> 16);
		/* Copy Config descriptor into payload */
		*(volatile u32*)(pma + 0x80) = *(u32 *)&desc_cfg[0];
		*(volatile u32*)(pma + 0x84) = *(u32 *)&desc_cfg[4];
		*(volatile u32*)(pma + 0x88) = *(u32 *)&desc_cfg[8];
		if (wLength == 9)
		{
			/* Update EP0 */
			*(volatile u32*)(pma + 0) = (9 << 16) | 0x80;
		}
		else if (wLength == 32)
		{
			/* Insert Interface Descriptor too */
			*(volatile u32*)(pma + 0x8C) = *(u32 *)&desc_cfg[12];
			*(volatile u32*)(pma + 0x90) = *(u32 *)&desc_cfg[16];
			*(volatile u32*)(pma + 0x94) = *(u32 *)&desc_cfg[20];
			*(volatile u32*)(pma + 0x98) = *(u32 *)&desc_cfg[24];
			*(volatile u32*)(pma + 0x9C) = *(u32 *)&desc_cfg[28];
			*(volatile u32*)(pma + 0xA0) = *(u32 *)&desc_cfg[32];
			/* Update EP0 */
			*(volatile u32*)(pma + 0) = (32 << 16) | 0x80;
		}
		else
			return;
	}
	/* Send Device Qualifier */
	else if (num == 6)
	{
		/* Copy payload */
		*(volatile u32*)(pma + 0x80) = *(u32 *)&desc_dev_qualifier[0];
		*(volatile u32*)(pma + 0x84) = *(u32 *)&desc_dev_qualifier[4];
		*(volatile u32*)(pma + 0x88) = *(u32 *)&desc_dev_qualifier[8];
		/* Update EP0 */
		*(volatile u32*)(pma + 0) = (10 << 16) | 0x80;
	}
	else
		return;
	/* Update EP0 for IN transfer */
	ep0r = reg_rd(USB_CHEPxR(0));
	ep0r &= ~(u32)(0x7040);
	ep0r |=  (u32)(1 << 15);
	ep0r ^= (1 << 4);
	ep0r ^= (1 << 5);
	reg_wr(USB_CHEPxR(0), ep0r);
#ifdef USB_DEBUG
	if (dbg_flags & DBG_EP0_REG)
	{
		readback = reg_rd(USB_CHEPxR(0));
		uart_puts(" - DESC TX "); uart_puthex(ep0r, 32);
		uart_puts(" > "); uart_puthex(readback, 32);
		uart_puts("\r\n");
	}
#endif
}

void ep0_rx(void)
{
	char buf_in[32];
	u8 *pma = (u8 *)USB_RAM;
	uint len;
	u16 wValue;
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
		uart_puts("EP0: GET_STATUS\r\n");
	/* GET_DESCRIPTOR */
	else if ((val & 0xFFFF) == 0x0680)
	{
		wValue = (u16)(val >> 16);
		/* Device Descriptor */
		if (wValue == 0x0100)
		{
#ifdef USB_INFO
			uart_puts("USB: Get Device Descriptor\r\n");
#endif
			state = 2;
			ep0_desc(1);
		}
		/* Configuration descriptor */
		else if (wValue == 0x0200)
		{
#ifdef USB_INFO
			uart_puts("USB: Get Configuration Descriptor\r\n");
#endif
			state = 3;
			ep0_desc(2);
		}
		/* String Descriptor */
		else if ((wValue & 0xFF00) == 0x0300)
			ep0_get_string();
		/* Device Qualifier  */
		else if (wValue == 0x0600)
		{
			uart_puts("USB: Get Device Qualifier\r\n");
			state = 5;
			ep0_desc(6);
		}
		else
		{
			uart_puts("USB: GET_DESCRIPTOR ");
			uart_puthex(wValue, 16);
			uart_puts("(Unknown)\r\n");
		}
	}
	/* SET_ADDRESS */
	else if ((val & 0xFFFF) == 0x0500)
	{
		dev_addr = ((val >> 16) & 0x7F);
		state = 1;
#ifdef USB_INFO
		uart_puts("USB: Set address ");
		uart_putdec(dev_addr);
		uart_puts("\r\n");
#endif
		/* Update EP0 */
		*(volatile u32*)(USB_RAM + 0) = (0 << 16) | 0x80;
		ep0r = reg_rd(USB_CHEPxR(0));
		ep0r &= ~(u32)(0x7040);
		ep0r |=  (u32)(1 << 15); // Keep VTRX (if set)
		ep0r &= ~(u32)(1 << 7);  // Clear VTTX (if set
		ep0r ^= (1 << 4); // STATTX0
		ep0r ^= (1 << 5); // STATTX1 : Valid
		reg_wr(USB_CHEPxR(0), ep0r);
	}
	/* SET_CONFIGURATION */
	else if ((val & 0xFFFF) == 0x0900)
	{
		wValue = (u16)(val >> 16);
		uart_puts("USB: Set Configuration ");
		uart_putdec(wValue);
		uart_puts("\r\n");
		/* Send ZLP (Status) */
		state = 4;
		*(volatile u32*)(USB_RAM + 0) = (0 << 16) | 0x80;
		ep0r = reg_rd(USB_CHEPxR(0));
		ep0r &= ~(u32)(0x7040);
		ep0r |=  (u32)(1 << 15); // Keep VTRX (if set)
		ep0r &= ~(u32)(1 << 7);  // Clear VTTX (if set
		ep0r ^= (1 << 4); // STATTX0
		ep0r ^= (1 << 5); // STATTX1 : Valid
		reg_wr(USB_CHEPxR(0), ep0r);
	}
	else
	{
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
	}

rx_end:
#ifdef USB_DEBUG
	/* Fill memory with pattern for later inspection */
	*(volatile u32*)(pma + 0x40) = 0xAA55AA55;
	*(volatile u32*)(pma + 0x44) = 0xA55AA55A;
	*(volatile u32*)(pma + 0x48) = 0xAA55AA55;
	*(volatile u32*)(pma + 0x4C) = 0xA55AA55A;
	*(volatile u32*)(pma + 0x50) = 0xAA55AA55;
	*(volatile u32*)(pma + 0x54) = 0xA55AA55A;
#endif
	*(u32*)(pma + 4) = (u32)((1 << 31) | (1 << 26) | (0 << 16) | 0x40);
	ep0r = reg_rd(USB_CHEPxR(0));
	ep0r &= ~(u32)(0x4070);  // Keep bits
	ep0r |=  (u32)(1 << 7);  // Keep VTTX (if set)
	ep0r &= ~(u32)(1 << 15); // Clear VTRX
	ep0r ^= (1 << 12); // STATRX0 = 1
	ep0r ^= (1 << 13); // STATRX1 = 1 (Valid)
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

void USB_Handler(void)
{
	u32  isr_ack = (1 << 9);
	uint ep, dir;
	u32  v = reg_rd(USB_ISTR);
	u32  ep0r;

#ifdef USB_DEBUG
	u32 readback;

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
		state = 0;
		/* Reset device address */
		reg_wr(USB_DADDR, (1 << 7));
		ep0_config(0);
		isr_ack = (1 << 10);
		isr_ack |= ((1 << 11) | (1 << 8));
	}
	/* CTR */
	else if (v & (1 << 15))
	{
		ep  = (v & 0x0F);
		dir = (v & (1 << 4)) ? 1 : 0;

		if (ep == 0)
		{
			if (dir == 1)
				ep0_rx();
			else
			{
#ifdef USB_DEBUG
				if (dbg_flags & DBG_EP0_REG)
					uart_puts("EP0: EndTX\r\n");
#endif
				if (state == 1)
				{
					state = 0;
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
					ep0r ^= (1 << 12); // STATRX0 = 1
					ep0r ^= (1 << 13); // STATRX1 = 1 (Valid)
					reg_wr(USB_CHEPxR(0), ep0r);
				}
				else if ((state >= 2) && (state <= 5))
				{
					state = 0;
					/* Update EP0 */
					*(volatile u32*)(USB_RAM + 0) = (0 << 16) | 0x80;
					ep0r = reg_rd(USB_CHEPxR(0));
					ep0r &= ~(u32)(0x7070);
					ep0r |=  (u32)(1 << 15);
					ep0r &= ~(u32)(1 << 7);
					reg_wr(USB_CHEPxR(0), ep0r);
#ifdef USB_DEBUG
					if (dbg_flags & DBG_EP0_REG)
					{
						readback = reg_rd(USB_CHEPxR(0));
						uart_puts(" - "); uart_puthex(ep0r, 32);
						uart_puts(" > "); uart_puthex(readback, 32);
						uart_puts("\r\n");
					}
#endif
					ep0r = reg_rd(USB_CHEPxR(0));
					ep0r &= ~(u32)(0x0070);
					ep0r |=  (u32)(1 << 7);
					ep0r ^= (1 << 12); // STATRX0
					ep0r ^= (1 << 13); // STATRX1 : Valid
					reg_wr(USB_CHEPxR(0), ep0r);
#ifdef USB_DEBUG
					if (dbg_flags & DBG_EP0_REG)
					{
						readback = reg_rd(USB_CHEPxR(0));
						uart_puts(" - "); uart_puthex(ep0r, 32);
						uart_puts(" > "); uart_puthex(readback, 32);
						uart_puts("\r\n");
					}
#endif
				}
				else
				{
					ep0r = reg_rd(USB_CHEPxR(0));
					ep0r &= ~(u32)(0x7070);
					ep0r |=  (u32)(1 << 15);
					ep0r &= ~(u32)(1 << 7);
					reg_wr(USB_CHEPxR(0), ep0r);
					uart_puts(" - "); uart_puthex(ep0r, 32); uart_puts(" > "); uart_puthex(reg_rd(USB_CHEPxR(0)), 32); uart_puts("\r\n");
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
