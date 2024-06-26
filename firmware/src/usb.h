/**
 * @file  usb.h
 * @brief Headers and definitions for STM32G0 USB driver
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
#ifndef USB_H
#define USB_H
#include "hardware.h"
#include "types.h"

#define USB_ST_POWERED    0
#define USB_ST_DEFAULT    1
#define USB_ST_ADDRESS    2
#define USB_ST_CONFIGURED 3

#define USB_RAM (USB_R1)
/* USB peripheral registers */
#define USB_CHEPxR(x) (u32)(USB + (x*4))
#define USB_CNTR      (u32)(USB + 0x40)
#define USB_ISTR      (u32)(USB + 0x44)
#define USB_FNR       (u32)(USB + 0x48)
#define USB_DADDR     (u32)(USB + 0x4C)
#define USB_LPMCSR    (u32)(USB + 0x54)
#define USB_BCDR      (u32)(USB + 0x58)

/* Endpoint types */
#define USB_EP_BULK    0
#define USB_EP_CONTROL 1
#define USB_EP_ISO     2
#define USB_EP_INT     3
/* Endpoint states */
#define USB_EP_STALL   1
#define USB_EP_NAK     2
#define USB_EP_VALID   3

typedef struct __attribute__((packed))
{
	u8  bmRequestType;
	u8  bRequest;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
} usb_ctrl_request;

/**
 * @brief USB interface description structure
 */
typedef struct usb_if_drv_s
{
	void (*periodic)(void);
	void (*reset)(void);
	void (*enable)(int cfg_id);
	int  (*ctrl_req)(usb_ctrl_request *req, uint len, u8 *data);
} usb_if_drv;

/**
 * @brief USB endpoint description structure
 */
typedef struct usb_ep_def_s
{
	int (*release)(const u8 ep);
	int (*rx)(u8 *data, uint len);
	int (*tx_complete)(void);
} usb_ep_def;

void usb_init(void);
void usb_start(void);
void usb_periodic(void);

void usb_send(const u8 ep, const u8 *data, unsigned int len);
void usb_ep_configure(u8 ep, u8 type, usb_ep_def *def);
void usb_ep_set_state(u8 ep, u8 state);
int  usb_if_register(uint num, usb_if_drv *new_if);

#endif
