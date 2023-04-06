/**
 * @file  usb_desc.h
 * @brief Definition of USB device descriptors
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
#ifndef USB_DESC_H
#define USB_DESC_H

#define USB_IF_COUNT  1
#define USB_EP_COUNT  7
#define USB_STR_COUNT 3

const u16 ep_offsets[8][2] = {
	{0x000, 0x000}, /* EP0 : Control  */
	{0x180, 0x140}, /* EP1 : Bulk IN  */
	{0x100, 0x0C0}, /* EP2 : Bulk OUT */
	{0    , 0    }, /* EP3 (unused) */
	{0    , 0    }, /* EP4 (unused) */
	{0    , 0    }, /* EP5 (unused) */
	{0    , 0    }, /* EP6 (unused) */
	{0    , 0    }  /* EP7 (unused) */
};

/**
 * @brief Device descriptor
 */
const u8 desc_device[] __attribute__((aligned(4))) = {
	18,   0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
	0x08, 0x36, 0x20, 0xC7, 0x01, 0x01, 0x01, 0x02,
	0x00, 0x01 };

/**
 * @brief Device qualifier
 */
const u8 desc_dev_qualifier[] __attribute__((aligned(4))) = {
	10,   0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
	0x00, 0x00 };

const u8 desc_cfg[] __attribute__((aligned(4))) = {
	/* ---- Configuration Descriptor ----*/
	0x09, 0x02,   32, 0x00, 0x01, 0x01, 0x00, 0x80,
	0xFA,
	/* ---- Interface Descriptor ---- */
	0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50,
	0x00,
	/* ---- Endpoint (01, Bulk IN) ---- */
	0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x01,
	/* ---- Endpoint (02, Bulk OUT) ---- */
	0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x01,
	};

const u8 usbdev_str_lang[] __attribute__((aligned(4))) = {
	4, 0x03,
	0x09, 0x04
};

const u8 usbdev_str_manuf[] __attribute__((aligned(4))) = {
	16, 0x03,
	'A',0x00, 'g',0x00, 'i',0x00, 'l',0x00,
	'a',0x00, 'c',0x00, 'k',0x00
};

const u8 usbdev_str_product[] __attribute__((aligned(4))) = {
	26, 0x03,
	'C',0x00, 'o',0x00, 'w',0x00, 's',0x00,
	't',0x00, 'i',0x00, 'c',0x00, 'k',0x00,
	'-',0x00, 'u',0x00, 'm',0x00, 's',0x00
};

const u8 *usbdev_strings[USB_STR_COUNT] = {
	usbdev_str_lang,
	usbdev_str_manuf,
	usbdev_str_product
};
#endif
/* EOF */
