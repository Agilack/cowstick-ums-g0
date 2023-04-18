/**
 * @file  usb_msc/usb_if.hpp
 * @brief Definition of the UsbIf, a class for communication with USB interface
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
#ifndef USB_IF_HPP
#define USB_IF_HPP
#include <cstdint>

class UsbIf
{
public:
	UsbIf();
	~UsbIf();
	int  read(uint8_t *data, int *len);
	void reset();
	int  resetRecovery();
	int  write(uint8_t *packet, int len);
private:
	void initLibusb();
	void openDevice(uint16_t vid, uint16_t pid);
	void selectInterface();
private:
	bool  mLibusbContext;
	bool  mKernelDetached;
	char  mLogMsg[256];
	void *mDev;
	int   mEpIn;
	int   mEpOut;
};
#endif
/* EOF */
