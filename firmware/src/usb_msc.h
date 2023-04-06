/**
 * @file  usb_msc.h
 * @brief Headers and definitions for Mass Storage class USB driver
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
#ifndef USB_MSC_H
#define USB_MSC_H
#include "types.h"

typedef struct  __attribute__((packed))
{
	u32 signature;
	u32 tag;
	u32 data_length;
	u8  flags;
	u8  lun;
	u8  cb_len;
	u8  cb[16];
	u8  rsv_pad;
} msc_cbw;

typedef struct  __attribute__((packed))
{
	u32 signature;
	u32 tag;
	u32 residue;
	u8  status;
} msc_csw;

void usb_msc_init(void);

#endif
/* EOF */
