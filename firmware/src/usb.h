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

#define USB_RAM (USB_R1)
/* USB peripheral registers */
#define USB_CHEPxR(x) (USB + (x*4))
#define USB_CHEP0R (USB + 0x00)
#define USB_CHEP1R (USB + 0x04)
#define USB_CNTR   (USB + 0x40)
#define USB_ISTR   (USB + 0x44)
#define USB_DADDR  (USB + 0x4C)
#define USB_BCDR   (USB + 0x58)

void usb_init(void);
void usb_start(void);
void usb_periodic(void);

#endif
