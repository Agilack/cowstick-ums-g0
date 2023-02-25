/**
 * @file  tests/usb_device/main.c
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

typedef libusb_device_handle usbdev;

int ep0_clear_feature(usbdev *dev, int feature);
int ep0_get_status(usbdev *dev);
int ep0_get_configuration(usbdev *dev);
int ep0_get_descriptor(usbdev *dev, int id);
int ep0_get_interface (usbdev *dev, int id);
int ep0_get_string(usbdev *dev, int id);
int ep0_set_descriptor(usbdev *dev);
int ep0_set_feature(usbdev *dev, int feature);
int ep0_set_interface(usbdev *dev, int index, int value);
void log_title(char *str);
void log_fail(void);
void log_success(void);

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
	//dev = libusb_open_device_with_vid_pid(NULL, 0x0bda, 0x5538);
	if (dev == 0)
	{
		perror("Cowstick device not found");
		goto complete;
	}

	ep0_get_status(dev);
	/* Get device descriptor */
	ep0_get_descriptor(dev, 1);
	/* Get configuration descriptor */
	ep0_get_descriptor(dev, 2);
	/* Get string descriptors */
	ep0_get_string(dev, 0);
	ep0_get_string(dev, 1);
	ep0_get_string(dev, 2);
	ep0_get_string(dev, 3);
	/* Get device qualifier */
	ep0_get_descriptor(dev, 6);
	/**/
	ep0_get_configuration(dev);
	ep0_get_interface(dev, 0);
	ep0_set_feature(dev, 1);
	ep0_clear_feature(dev, 1);
	ep0_set_interface(dev, 1, 1);
	ep0_set_descriptor(dev);
	/* Try to get an unknown/invalid descriptor */
	ep0_get_descriptor(dev, 0);

complete:
	if (dev)
	{
		libusb_close(dev);
		dev = 0;
	}
	libusb_exit(NULL);
}

int ep0_get_configuration(usbdev *dev)
{
	uint8_t  bmRequestType = 0x80;
	uint8_t  bRequest      = 0x08;
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 1;
	unsigned char data[4]  = {0};
	unsigned int  timeout  = 500;
	int result;

	log_title("GET_CONFIGURATION");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result > 0)
	{
		int i;
		for (i = 0; i < result; i++)
			printf(" %.2X", data[i]);
	}
	else if (result == 0)
	{
		printf("Request ok but empty response");
		log_fail();
		return(result);
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

int ep0_get_descriptor(usbdev *dev, int id)
{
	uint8_t  bmRequestType  = 0x80;
	uint8_t  bRequest       = 0x06;
	uint16_t wValue         = 0x0000;
	uint16_t wIndex         = 0x0000;
	uint16_t wLength        = 0;
	unsigned char data[256] = {0};
	unsigned int  timeout   = 500;
	int result;

	wValue = (id << 8);
	if (id == 1)
		wLength = 18;
	else if (id == 2)
		wLength = 9;
	else if (id == 3)
		wLength = 255;
	else if (id == 6)
		wLength = 255;

	log_title("GET_DESCRIPTOR");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result > 0)
	{
		int i;
		printf("(%.2X)", id);
		for (i = 0; i < result; i++)
			printf(" %.2X", data[i]);
	}
	else if (result == 0)
	{
		printf("Request ok but empty response");
		log_fail();
		return(result);
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

int ep0_get_interface(usbdev *dev, int id)
{
	uint8_t  bmRequestType = 0x80;
	uint8_t  bRequest      = 0x0A;
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 1;
	unsigned char data[4] = {0};
	unsigned int  timeout  = 500;
	int result;

	wValue = (id << 8);

	log_title("GET_INTERFACE");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result > 0)
	{
		int i;
		printf("(%.2X)", id);
		for (i = 0; i < result; i++)
			printf(" %.2X", data[i]);
	}
	else if (result == 0)
	{
		printf("Request ok but empty response");
		log_fail();
		return(result);
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

int ep0_get_string(usbdev *dev, int id)
{
	uint8_t  bmRequestType  = 0x80; /* Device to Host */
	uint8_t  bRequest       = 0x06; /* GET_DESCRIPTOR */
	uint16_t wValue         = 0x0300; /* String descriptor */
	uint16_t wIndex         = 0x0000;
	uint16_t wLength        = 255;
	unsigned char data[256] = {0};
	unsigned int  timeout   = 500;
	int result;

	wValue |= (id & 0xFF);

	log_title("GET_DESCRIPTOR");
	printf("String %d : ", id);

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result > 0)
	{
		int i;
		if (id > 0)
		{
			for (i = 2; i < result; i+=2)
				printf("%c", data[i]);
		}
		else
		{
			for (i = 0; i < result; i++)
				printf(" %.2X", data[i]);
		}
	}
	else if (result == 0)
	{
		printf("Request ok but empty response");
		log_fail();
		return(result);
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

int ep0_get_status(usbdev *dev)
{
	int result;
	unsigned char value[2] = {0};

	log_title("GET_STATUS");

	result = libusb_control_transfer(dev, 0x80, 0x00, 0, 0, value, 2, 100);
	if (result < 0)
	{
		printf("failed %d [%s]", result, libusb_strerror(result));
		log_fail();
		return(1);
	}
	else if (result == 0)
	{
	}
	else
	{
		printf("%.2X%.2x", value[1], value[0]);
	}
	log_success();
	return(0);
}

int ep0_set_descriptor(usbdev *dev)
{
	uint8_t  bmRequestType = 0x00; /* Host to Device */
	uint8_t  bRequest      = 0x07; /* SET_DESCRIPTOR */
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 0;
	unsigned char data[4]  = {0};
	unsigned int  timeout  = 500;
	int result;

	wValue  = 1; /* Descriptor index */
	wIndex  = 0; /* Language ID */
	wLength = 4;

	log_title("SET_DESCRIPTOR");
	printf("(expect a Request Error)");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result == -9)
	{
		/* Normal case */
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

int ep0_set_feature(usbdev *dev, int feature)
{
	uint8_t  bmRequestType = 0x00; /* Host to Device */
	uint8_t  bRequest      = 0x03; /* SET_FEATURE */
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 0;
	unsigned char data[4]  = {0};
	unsigned int  timeout  = 500;
	int result;

	wValue |= (feature & 0xFF);

	log_title("SET_FEATURE");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result == 0)
	{
		/* Normal case */
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

int ep0_set_interface(usbdev *dev, int index, int value)
{
	uint8_t  bmRequestType = 0x00; /* Host to Device */
	uint8_t  bRequest      = 0x0B; /* SET_INTERFACE */
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 0;
	unsigned char data[4]  = {0};
	unsigned int  timeout  = 500;
	int result;

	wIndex = index;
	wValue = value;

	log_title("SET_INTERFACE");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result == 0)
	{
		/* Normal case */
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

int ep0_clear_feature(usbdev *dev, int feature)
{
	uint8_t  bmRequestType = 0x00; /* Host to Device */
	uint8_t  bRequest      = 0x01; /* CLEAR_FEATURE */
	uint16_t wValue        = 0x0000;
	uint16_t wIndex        = 0x0000;
	uint16_t wLength       = 0;
	unsigned char data[4]  = {0};
	unsigned int  timeout  = 500;
	int result;

	wValue |= (feature & 0xFF);

	log_title("CLEAR_FEATURE");

	result = libusb_control_transfer(dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
	if (result == 0)
	{
		/* Normal case */
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

/**
 * @brief Print a title for the current test
 *
 * @param str Pointer to a string with the title to print
 */
void log_title(char *str)
{
	printf("       ");
	printf(str);
	printf(" ");
}

/**
 * @brief Print a failure result for the current test
 *
 */
void log_fail(void)
{
	printf("\r[");
	printf("\x1B[31m"); /* Red */
	printf("FAIL");
	printf("\x1B[0m");  /* No Color */
	printf("] ");
	printf("\r\n");
}

/**
 * @brief Print a success result for the current test
 *
 */
void log_success(void)
{
	printf("\r[");
	printf("\x1B[32m"); /* Green */
	printf("PASS");
	printf("\x1B[0m");  /* No Color */
	printf("] ");
	printf("\r\n");
}
/* EOF */
