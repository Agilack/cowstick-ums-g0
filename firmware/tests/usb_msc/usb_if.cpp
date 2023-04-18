/**
 * @file  usb_msc/usb_if.cpp
 * @brief The UsbIf class handle communication with one device interface
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
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <libusb-1.0/libusb.h>
#include "params.hpp"
#include "usb_if.hpp"

/**
 * @brief Default constructor
 *
 */
UsbIf::UsbIf()
{
	uint16_t vid, pid;
	mDev = 0;
	mLibusbContext  = false;
	mKernelDetached = false;
	mEpIn  = -1;
	mEpOut = -1;

	initLibusb();

	vid = Params::getVid();
	pid = Params::getPid();
	openDevice(vid, pid);

	selectInterface();

	reset();
}

/**
 * @brief Default object destructor
 *
 */
UsbIf::~UsbIf()
{
	libusb_device *dev;

	if (mDev)
	{
		dev = libusb_get_device((libusb_device_handle *)mDev);
		libusb_close((libusb_device_handle*)mDev);
		libusb_unref_device(dev);
	}
	if (mLibusbContext)
		libusb_exit(NULL);
}

int UsbIf::read(uint8_t *data, int *len)
{
	libusb_device_handle *dev = (libusb_device_handle *)mDev;
	unsigned int  timeout = 2000;
	int transfered;
	int result;

	if (len == 0)
		throw std::runtime_error("UsbIf: Read with null length");

	result = libusb_bulk_transfer(dev, mEpIn, data, *len, &transfered, timeout);
	if (result == 0)
	{
		printf("UsbIf: read length=%d => transfered=%d\n", *len, transfered);
		*len = transfered;
	}
	else if (result == -9)
	{
		printf("UsbIf: read STALL, clear ep\n");
		libusb_clear_halt(dev, mEpIn);
	}
	else
	{
		printf("UsbIf: read failed %d [%s]\n", result, libusb_strerror(result));
		*len = 0;
	}
	return(result);
}

/**
 * @brief Send an USB (software) reset to device
 *
 */
void UsbIf::reset()
{
	libusb_device_handle *handle;

	if (mDev == 0)
		return;

	handle = (libusb_device_handle *)mDev;

	// Reset device
	libusb_reset_device(handle);
}

/**
 * @brief Perform a Reset Recovery sequence
 *
 * The reset recovery sequence is a Bulk-Only mass storage operation (usb class
 * specific command) used to clear some errors and make the device ready for
 * the next CBW command.
 *
 * @return integer Zero is returned on success, other values are errors
 */
int UsbIf::resetRecovery()
{
	libusb_device_handle *dev = (libusb_device_handle *)mDev;
	uint8_t  bmRequestType = 0x21; /* Host to Device, class interface */
	uint8_t  bRequest      = 0xFF; /* ResetRecovery */
	uint16_t wIndex        = 0x0000; // TODO use BulkOnly interface id
	int result;
	unsigned char value[2] = {0};

	// a) Send a Bulk-Only Mass Storage Reset
	result = libusb_control_transfer(dev, bmRequestType, bRequest, 0, wIndex, value, 0, 500);
	if (result < 0)
	{
		printf("ResetRecovery error %d [%s]\n", result, libusb_strerror(result));
		return(-1);
	}

	// b) Clear HALT of the bulk-in endpoint
	libusb_clear_halt(dev, mEpIn);
	// c) Clear HALT of the bulk-out endpoint
	libusb_clear_halt(dev, mEpOut);

	return(0);
}

int UsbIf::write(uint8_t *packet, int len)
{
	libusb_device_handle *dev = (libusb_device_handle *)mDev;
	unsigned int  timeout = 2000;
	int transfered;
	int result;

	result = libusb_bulk_transfer(dev, mEpOut, packet, len, &transfered, timeout);
	if (result == -9)
	{
		printf("UsbIf: write STALL, clear ep\n");
		libusb_clear_halt(dev, mEpOut);
	}
	else if (result != 0)
		printf("UsbIf: Write failed %d [%s]\n", result, libusb_strerror(result));
	else if (transfered != len)
		printf("UsbIf: Write complete, transfered %d != expected %d\n", transfered, len);

	return(result);
}

/* -------------------------------------------------------------------------- */
/* --                                                                      -- */
/* --                   Protected and Private  functions                   -- */
/* --                                                                      -- */
/* -------------------------------------------------------------------------- */

void UsbIf::initLibusb()
{
	if (libusb_init_context(NULL, NULL, 0))
		throw std::runtime_error("Failed to init libusb");

	mLibusbContext = true;
}

void UsbIf::openDevice(uint16_t vid, uint16_t pid)
{
	libusb_device **list, *dev;
	libusb_device_descriptor desc;
	libusb_device_handle *handle;
	ssize_t count;
	int result;
	int i;

	/* Get full list of available devices */
	count = libusb_get_device_list(NULL, &list);
	if (count < 0)
		throw std::runtime_error("Failed to get list of USB devices");
	/* Parse device list to find specified device */
	printf("0.1) Search specified USB device\n");
	dev = 0;
	for (i = 0; i < count; i++)
	{
		libusb_device *device = list[i];
		libusb_get_device_descriptor(device, &desc);
		sprintf(mLogMsg, " - %.4X:%.4X", desc.idVendor, desc.idProduct);
		if ((desc.idVendor == vid) && (desc.idProduct == pid))
		{
			if (dev == 0)
			{
				libusb_ref_device(device);
				dev = device;
				strcat(mLogMsg, " \x1B[1;32m(found)\x1B[0m");
			}
			else
				strcat(mLogMsg, " \x1B[1;33m(dup)\x1B[0m");
		}
		strcat(mLogMsg, "\n");
		printf(mLogMsg);
	}
	// End of list parsing, free it
	libusb_free_device_list(list, 1);

	if (dev == 0)
	{
		char emsg[64];
		sprintf(emsg, "USB device not found (%.4X:%.4X)", vid, pid);
		throw std::runtime_error(emsg);
	}

	printf("0.2) Try to open selected usb device\n");

	result = libusb_open(dev, &handle);
	if (result)
	{
		// In case of failure, decrement device reference count
		libusb_unref_device(dev);
		// Return the error
		if (result == LIBUSB_ERROR_ACCESS)
			throw std::runtime_error("Failed to open device: permission denied");
		else if (result == LIBUSB_ERROR_NO_DEVICE)
			throw std::runtime_error("Failed to open device: disconnected");
		else
		{
			char emsg[64];
			sprintf(emsg, "Failed to open device: unknown error %d", result);
			throw std::runtime_error(emsg);
		}
	}
	mDev = (void *)handle;
}

void UsbIf::selectInterface()
{
	struct libusb_config_descriptor *config;
	libusb_device_handle *handle;
	libusb_device *dev;
	int if_num = -1;
	int result;
	int i, j;

	// Get current configuration descriptor for device
	handle = (libusb_device_handle *)mDev;
	dev    = libusb_get_device(handle);
	result = libusb_get_active_config_descriptor(dev, &config);
	if (result < 0)
		throw std::runtime_error("UsbIf: Failed to get current usb configuration");

	printf("0.3) Search MSC interface into USB device\n");
	// Parse all interfaces to search MSC
	for (i = 0; i < config->bNumInterfaces; i++)
	{
		const struct libusb_interface *itf = &config->interface[i];
		for (j = 0; j < itf->num_altsetting; j++)
		{
			const struct libusb_interface_descriptor *itf_desc;
			itf_desc = &itf->altsetting[j];
			bool itf_class    = (itf_desc->bInterfaceClass    == 0x08) ? 1 : 0;
			bool itf_subclass = (itf_desc->bInterfaceSubClass == 0x06) ? 1 : 0;
			bool itf_proto    = (itf_desc->bInterfaceProtocol == 0x50) ? 1 : 0;
			sprintf(mLogMsg, " - If %d Class=%.2X Subclass=%.2X Protocol=%.2X",
			        i, itf_desc->bInterfaceClass, itf_desc->bInterfaceSubClass,
			        itf_desc->bInterfaceProtocol);
			if (itf_class && itf_subclass && itf_proto)
			{
				if (if_num < 0)
				{
					const struct libusb_endpoint_descriptor *ep_desc;
					if_num = itf_desc->bInterfaceNumber;
					strcat(mLogMsg, " \x1B[1;32m(found)\x1B[0m");

					// TODO : Test number of available endpoints before accessing [0] / [1]

					// Get info about first endpoint
					ep_desc = &itf_desc->endpoint[0];
					if (ep_desc->bEndpointAddress & 0x80)
						mEpIn = ep_desc->bEndpointAddress;
					else
						mEpOut = ep_desc->bEndpointAddress;
					// Get info about second endpoint
					ep_desc = &itf_desc->endpoint[1];
					if (ep_desc->bEndpointAddress & 0x80)
						mEpIn = ep_desc->bEndpointAddress;
					else
						mEpOut = ep_desc->bEndpointAddress;
				}
				else
					strcat(mLogMsg, " \x1B[1;33m(dup)\x1B[0m");
			}
			strcat(mLogMsg, "\n");
			printf(mLogMsg);
		}
	}
	// End of configs parsing, free them
	libusb_free_config_descriptor(config);

	if (if_num < 0)
		throw std::runtime_error("Failed to find MSC interface into device");

	// If the device interface is already used by a kernel driver, try to release it
	if (libusb_kernel_driver_active(handle, if_num))
	{
		result = libusb_detach_kernel_driver(handle, if_num);
		if (result == 0)
			mKernelDetached = true;
	}

	// Try to get access to MSC interface
	result = libusb_claim_interface(handle, if_num);
	if (result)
		throw std::runtime_error("Failed to claim interface");
}
/* EOF */
