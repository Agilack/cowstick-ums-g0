/**
 * @file  tests/usb_bulk/main.c
 * @brief Entry point of the unit-test program
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#include "log.h"

typedef libusb_device_handle usbdev;

int bulk_rd(usbdev *dev);
int bulk_wr(usbdev *dev, int len);

int main (int argc, char **argv)
{
	libusb_device_handle *dev = 0;

	(void)argc;
	(void)argv;

	if (libusb_init_context(NULL, NULL, 0))
	{
		perror("Failed to init libusb\n");
		return(-1);
	}
	dev = libusb_open_device_with_vid_pid(NULL, 0x3608, 0xc720);
	if (dev == 0)
	{
		perror("Cowstick device not found");
		goto complete;
	}

	bulk_wr(dev, 8);
	bulk_rd(dev);
	bulk_wr(dev, 64);
	bulk_rd(dev);
	bulk_wr(dev, 512);
	bulk_rd(dev);

complete:
	if (dev)
	{
		libusb_close(dev);
		dev = 0;
	}
	libusb_exit(NULL);
}

int bulk_rd(usbdev *dev)
{
	unsigned char ep = 0x81;
	unsigned char data[32];
	int length, transfered;
	unsigned int  timeout  = 500;
	int result;

	log_title("BULK_RD");

	length = 32;

	result = libusb_bulk_transfer(dev, ep, data, length, &transfered, timeout);
	if (result == 0)
	{
		printf("length=%d => transfered=%d", length, transfered);
	}
	else
	{
		printf("failed %d [%s]", result, libusb_strerror(result));
		log_fail();
		return(result);
	}
	log_success();
	return(0);
}

int bulk_wr(usbdev *dev, int len)
{
	unsigned char ep = 0x02;
	unsigned char *data = 0;
	int length, transfered;
	unsigned int  timeout  = 500;
	int result;
	int i;

	log_title("BULK_WR");

	data = (unsigned char *)malloc(len);
	for (i = 0; i < len; i++)
		data[i] = (i & 0xFF);

	length = len;

	result = libusb_bulk_transfer(dev, ep, data, length, &transfered, timeout);
	if (result == 0)
	{
		printf("length=%d => transfered=%d", length, transfered);
	}
	else
	{
		printf("failed %d [%s]", result, libusb_strerror(result));
		log_fail();
		if(data)
			free(data);
		return(result);
	}
	log_success();
	if (data)
		free(data);
	return(0);
}
