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

uint flag = 0;
uint it_count;

void ep0_desc(void);

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

	it_count = 0;

	/* Activate USB */
	reg_set((u32)RCC_APBENR1, (1 << 13));

	// Clear PDWN
	reg_clr((u32)USB_CNTR, 2);
	for (i = 0; i < 0x4000; i++)
		asm volatile("nop");

	/* Force reset */
	reg_wr((u32)USB_CNTR, (1 << 0));
	(void)reg_rd((u32)USB_CNTR);
	/* Release reset */
	reg_wr((u32)USB_CNTR, 0);
	(void)reg_rd((u32)USB_CNTR);

	for (i = 0; i < 0x4000; i++)
		asm volatile("nop");

	// Configure descriptors for EP0
	*(u32*)(pma + 0) = (0 << 16) | 0x60;
	*(u32*)(pma + 4) = (16 << 26) | (32 << 16) | 0x40;
	// Set EP0 as control
	v  = (1 << 9);  // UTYPE: Control
	v |= (3 << 12); // STATRX: Valid (wait for rx)
	v |= (2 <<  4); // STATTX: NAK
	reg_wr((u32)USB_CHEP0R, v);

	/* Enable USB interrupt into NVIC */
	reg_wr(0xE000E100, (1 << 8)); /* USB */

	uart_puts("USB Initialized\r\n");
}

void usb_start(void)
{
	u32 v;

	flag = 0;

	/* Set device address to 0 */
	reg_wr(USB_DADDR, (1 << 7));

	// configure interrupts
	reg_wr((u32)USB_ISTR, 0);
	v = (1 << 10); // RST_DCOM
	v |= (1 << 15); // CTR
	v |= (1 << 13); // ERR
	//v |= (1 << 11); // SUSP
	reg_wr((u32)USB_CNTR, v);

	reg_wr((u32)USB_BCDR, (1 << 15));

	uart_puts("USB Started\r\n");
}

void usb_periodic(void)
{
	if (flag == 1)
	{
		flag = 2;
		ep0_desc();
	}
}

void ep0_config(uint tx_rdy)
{
	u32 cur, v;

	cur = reg_rd(USB_CHEP0R);

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
	reg_wr((u32)USB_CHEP0R, v);

	uart_puts(" - CHEP0R: "); uart_puthex(reg_rd((u32)USB_CHEP0R), 32); uart_puts("\r\n");
}

const u8 desc1[] = {
	18,   0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 64,
	0x08, 0x36, 0x20, 0xC7, 0x01, 0x01, 0x01, 0x02,
	0x03, 0x01 };

void ep0_desc(void)
{
	u32 ep0r, copy, readback;
	u32 pma = USB_RAM;

	/* Copy payload */
	*(volatile u32*)(pma + 0x60) = *(u32 *)&desc1[0];
	*(volatile u32*)(pma + 0x64) = *(u32 *)&desc1[4];
	*(volatile u32*)(pma + 0x68) = *(u32 *)&desc1[8];
	*(volatile u32*)(pma + 0x6C) = *(u32 *)&desc1[12];
	*(volatile u32*)(pma + 0x70) = *(u32 *)&desc1[16];
	/* Update EP0 */
	*(volatile u32*)(pma + 0) = (18 << 16) | 0x60;
	/**/
	ep0r = reg_rd(USB_CHEP0R);
	copy = ep0r;
	ep0r &= ~(u32)(0x7040);
	ep0r |=  (u32)(1 << 15);
	ep0r ^= (1 << 4);
	ep0r ^= (1 << 5);
	reg_wr((u32)USB_CHEP0R, ep0r);
	readback = reg_rd((u32)USB_CHEP0R);
	uart_puts(" - TX CHEP0R "); uart_puthex(copy, 32); uart_puts(" > ("); uart_puthex(ep0r, 32); uart_puts(") "); uart_puthex(readback, 32); uart_puts("\r\n");
}

void ep0_rx(void)
{
	char buf_in[32];
	char buffer[16];
	u8 *pma = (u8 *)USB_RAM;
	u16 wValue;
	u32 val;
	u32 ep0r;

	*(u32 *)&buf_in[0]  = *(u32*)(pma + 0x40);
	*(u32 *)&buf_in[4]  = *(u32*)(pma + 0x44);
	*(u32 *)&buf_in[8]  = *(u32*)(pma + 0x48);
	*(u32 *)&buf_in[12] = *(u32*)(pma + 0x4C);

	val = *(u32*)(pma + 0x40);
	// GET_STATUS
	if ((val & 0xFFFF) == 0x0080)
		uart_puts("EP0: GET_STATUS\r\n");
	// GET_DESCRIPTOR
	else if ((val & 0xFFFF) == 0x0680)
	{
		uart_puts("EP0: GET_DESCRIPTOR\r\n");
		wValue = (u16)(val >> 16);
		/* Device Descriptor */
		if (wValue == 0x0100)
		{
			uart_puts(" - Device descriptor\r\n");
			if (flag == 0)
				ep0_desc();
		}
		/* Configuration descriptor */
		else if (wValue == 0x0200)
			uart_puts("Configuration descriptor");
		/* String Descriptor */
		else if (wValue == 0x0300)
			uart_puts("String descriptor");
		else
		{
			uart_puts("Unknown descriptor ");
			uart_puthex(wValue, 16);
		}
	}
	else
	{
		// Extract data
		val = *(u32*)(pma + 0x40);
		*(u32 *)&buffer[0] = val;
		val = *(u32*)(pma + 0x44);
		*(u32 *)&buffer[4] = val;
		uart_dump((u8*)buffer, 16);
	}

	*(volatile u32*)(pma + 0x40) = 0;
	*(volatile u32*)(pma + 0x44) = 0;
	*(volatile u32*)(pma + 0x48) = 0;
	*(volatile u32*)(pma + 0x4C) = 0;


	*(u32*)(pma + 4) = (16 << 26) | (32 << 16) | 0x40;
	ep0r = reg_rd(USB_CHEP0R);
	ep0r &= ~(u32)(0x7070); // Keep bits
	ep0r |=  (u32)(1 << 7);  // Keep VTTX (if set)
	ep0r &= ~(u32)(1 << 15); // Clear VTRX
	ep0r ^= (1 << 12); // STATRX0 = 1
	ep0r ^= (1 << 13); // STATRX1 = 1 (Valid)
	reg_wr((u32)USB_CHEP0R, ep0r);
	uart_puts(" - "); uart_puthex(ep0r, 32); uart_puts(" > "); uart_puthex(reg_rd((u32)USB_CHEP0R), 32); uart_puts("\r\n");
}

void USB_Handler(void)
{
	u32  isr_ack = (1 << 9);
	uint ep, dir;
	u32  v = reg_rd((u32)USB_ISTR);
	u32  ep0r;

	uart_puts("USB: IT ");
	uart_puthex(v, 32);
	uart_puts(" / CHEP0R ");
	uart_puthex(reg_rd(USB_CHEP0R), 32);
	uart_puts("\r\n");

	/* RST_DCON event */
	if (v & (1 << 10))
	{
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
				uart_puts("EP0: EndTX\r\n");
				uart_puts(" - CHEP_TX "); uart_puthex(*(volatile u32*)(USB_RAM + 0), 32);
				uart_puts("\r\n");
				if (flag == 2)
				{
					u32 readback;
					flag = 3;
					/* Update EP0 */
					*(volatile u32*)(USB_RAM + 0) = (0 << 16) | 0x60;
					ep0r = reg_rd(USB_CHEP0R);
					ep0r &= ~(u32)(0x7040);
					ep0r |=  (u32)(1 << 15);
					ep0r ^= (1 << 4); // STATTX0
					ep0r ^= (1 << 5); // STATTX1 : Valid
					ep0r &= ~(u32)(1 << 7);
					reg_wr((u32)USB_CHEP0R, ep0r);
					readback = reg_rd((u32)USB_CHEP0R);
					uart_puts(" - "); uart_puthex(ep0r, 32); uart_puts(" > "); uart_puthex(readback, 32); uart_puts("\r\n");
				}
				else
				{
					ep0r = reg_rd(USB_CHEP0R);
					ep0r &= ~(u32)(0x7070);
					ep0r |=  (u32)(1 << 15);
					ep0r &= ~(u32)(1 << 7);
					reg_wr((u32)USB_CHEP0R, ep0r);
					uart_puts(" - "); uart_puthex(ep0r, 32); uart_puts(" > "); uart_puthex(reg_rd((u32)USB_CHEP0R), 32); uart_puts("\r\n");
				}
			}
		}
		isr_ack = (1 << 15);
	}
	reg_wr((u32)USB_ISTR, ~isr_ack);
	it_count ++;
	if (it_count == 6)
		/* Disable USB interrupt into NVIC */
		reg_wr(0xE000E180, (1 << 8)); /* USB */
}
/* EOF */
