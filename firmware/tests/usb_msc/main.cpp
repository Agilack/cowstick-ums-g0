/**
 * @file  usb_msc/main.c
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
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "params.hpp"
#include "usb_if.hpp"
#include "cbw.hpp"
#include "csw.hpp"

#undef VENDOR

int test_HnDn(UsbIf *usbdev);
int test_HnDi(UsbIf *usbdev);
int test_HiDn(UsbIf *usbdev);
int test_HiDi(UsbIf *usbdev);
int test_HoDi(UsbIf *usbdev);
int test_HoDo(UsbIf *usbdev);
int test_recovery(UsbIf *usbdev);

int main(int argc, char **argv)
{
	int err_count = 0;

	if (Params::loadCmdline(argc, argv))
		return(0);

	try {
		UsbIf usbdev;

		// Case #1  : Hn = Dn
		err_count += test_HnDn(&usbdev);
		// Case #2  : Hn < Di
		err_count += test_HnDi(&usbdev);
		// Case #3  : Hn < Do  not implemented yet

		// Case #4  : Hi > Dn
//		err_count += test_HiDn(&usbdev);
		// Case #5  : Hi > Di  not implemented yet
		// Case #6  : Hi = Di
		err_count += test_HiDi(&usbdev);
		// Case #7  : Hi < Di  not implemented yet
		// Case #8  : Hi <> Do not implemented yet

		// Case #9  : Ho > Dn  not implemented yet
		// Case #10 : Ho <> Di not implemented yet
		// Case #11 : Ho > Do  not implemented yet
		// Case #12 : Ho = Do
		err_count += test_HoDo(&usbdev);
		// Case #13 : Ho < Do not implemented yet
	} catch (std::exception &e) {
		std::cout << e.what() << std::endl;
	};
}

void recovery(UsbIf *usbdev)
{
	// First, try a class specific reset procedure
	if (usbdev->resetRecovery() == 0)
	{
		// Verify if device works after reset recovery
		if (test_recovery(usbdev))
			// If no :( process a full device reset
			usbdev->reset();
	}
	else
		// ResetRecovery fails, so process a full device reset
		usbdev->reset();
}

int test_HnDn(UsbIf *usbdev)
{
	uint32_t tag = 0xBABE0001;
	int result;

	std::cout << std::endl << "\x1B[1;36m"
	          << "Test Hn = Dn (case #1)"
	          << "\x1B[0m" << std::endl;

	try
	{
		// Use a TEST UNIT READY command to test HnDn
		Cbw *c = new Cbw(0x80);
		c->setTag(tag);
		c->setCB((uint8_t *)"\x00\x00\x00\x00\x00\x00", 6);

		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write failed");

		uint8_t buffer[1024];
		int read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("read CSW");
		Csw csw(buffer, read_len);

		if ( ! csw.checkSignature())
			throw std::runtime_error("bad CSW signature");
		if (csw.getTag() != tag)
			throw std::runtime_error("bad tag response");
		if (csw.getStatus() == 2)
			throw std::runtime_error("Device report a 0x02 status");
	}
	catch(std::exception &e)
	{
		std::cout << "\x1B[1;31m" << e.what() << "\x1B[0m" << std::endl;
		recovery(usbdev);
		return(1);
	}
	std::cout << "\x1B[1;32m" << "Test Hn=Dn success"
	          << "\x1B[0m" << std::endl;

	return(0);
}

int test_HnDi(UsbIf *usbdev)
{
	Cbw *c;
	uint32_t tag = 0xBABE0002;
	int result;
	int read_len;
	uint8_t buffer[1024];

	std::cout << std::endl << "\x1B[1;36m"
	          << "Test Hn < Di (case #2)"
	          << "\x1B[0m" << std::endl;

	try
	{
		// Use a READ CAPACITY(10) command to test HnDi
		c = new Cbw(0x80);
		c->setTag(tag);
		c->setLength(0);
		c->setCB((uint8_t *)"\x25\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);

		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write failed");

		read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("Test HnDi failed to read CSW");
		if (read_len == 13)
		{
			Csw csw(buffer, read_len);

			if ( ! csw.checkSignature())
				throw std::runtime_error("Test HnDi failed, bad CSW signature");
			if (csw.getTag() != tag)
				throw std::runtime_error("Test HnDi failed, bad tag response");
		}
		else if (read_len > 0)
		{
			read_len = 1024;
			result = usbdev->read(buffer, &read_len);
			throw std::runtime_error("Test HnDi failed, data phase with Hn (!)");
		}
	}
	catch(std::exception &e)
	{
		std::cout << "\x1B[1;31m" << e.what() << "\x1B[0m" << std::endl;
		recovery(usbdev);
		return(1);
	}
	std::cout << "\x1B[1;32m" << "Test Hn<Di success"
	          << "\x1B[0m" << std::endl;

	return(0);
}

int test_HiDn(UsbIf *usbdev)
{
	Cbw *c;
	uint32_t tag = 0xBABE0004;
	int result;
	int read_len;
	uint8_t buffer[1024];
	uint32_t dummy_len = 8;

	std::cout << std::endl
	          << "\x1B[1;36m" << "Test Hi > Dn (case #4)"
	          << "\x1B[0m" << std::endl;

	try
	{
		// Use a TEST UNIT READY command to test HnDn
		c = new Cbw(0x80);
		c->setTag(tag);
		c->setLength(dummy_len); // Ask for data phase (!)
		c->setCB((uint8_t *)"\x00\x00\x00\x00\x00\x00", 6);

		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write failed");

		read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result == -9)
		{
			std::cout << " - STALL during data phase (good !)" << std::endl;
			read_len = 1024;
			result = usbdev->read(buffer, &read_len);
			Csw csw(buffer, read_len);

			if ( ! csw.checkSignature())
				throw std::runtime_error("Test HiDn failed, bad CSW signature");
			if (csw.getTag() != tag)
				throw std::runtime_error("Test HiDn failed, bad tag response");
			if (csw.getResidue() != dummy_len)
				throw std::runtime_error("Test HiDn failed, bad residue length");
		}
		else if (result < 0)
			throw std::runtime_error("Test HiDn failed to read CSW");
		else if (read_len == 13)
		{
			Csw csw(buffer, read_len);

			if ( ! csw.checkSignature())
				throw std::runtime_error("Test HiDn failed, no STALL and bad CSW signature");
			if (csw.getTag() != tag)
				throw std::runtime_error("Test HiDn failed, no STALL and bad tag response");
			if (csw.getResidue() != dummy_len)
				throw std::runtime_error("Test HiDn failed, no STALL and bad residue length");

			throw std::runtime_error("Test HiDn failed, receive a CSW without STALL data phase");
		}
		else
		{
			printf(" - Read result %d with %d bytes\n", result, read_len);
			throw std::runtime_error("Test HiDn failed, unexpected response");
		}
	}
	catch(std::exception &e)
	{
		std::cout << "\x1B[1;31m" << e.what() << "\x1B[0m" << std::endl;
		return(1);
	}
	std::cout << "\x1B[1;32m" << "Test Hi>Dn success"
	          << "\x1B[0m" << std::endl;

	return(0);
}

int test_HiDi(UsbIf *usbdev)
{
	Cbw *c;
	Csw *csw;
	uint32_t tag = 0xBABE0006;
	int result;
	uint8_t buffer[1024];
	int read_len;

	std::cout << std::endl << "\x1B[1;36m"
	          << "Test Hi = Di (case #6)"
	          << "\x1B[0m" << std::endl;

	try
	{
		// Use a READ CAPACITY(10) command to test Hi=Di
		c = new Cbw(0x80);
		c->setTag(tag);
		c->setLength(8);
		c->setCB((uint8_t *)"\x25\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);

		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write failed");

		// Try to read returned data
		read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("Data read failed");
		if ((result == 0) && (read_len == 8))
		{
			for (int i = 0; i < 8; i++)
			{
				printf("%.2X", buffer[i]);
				if ((i % 16) == 15)
					printf("\n");
				else
					printf(" ");
			}
			printf("\n");
		}
		else if ((result == 0) && (read_len == 13))
		{
			csw = new Csw(buffer, read_len);

			if ( ! csw->checkSignature())
				throw std::runtime_error("Test HiDi failed, bad CSW signature\n");
			return(1);
		}

		// Now, try to read CSW
		read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("Read CSW failed");
		csw = new Csw(buffer, read_len);

		if ( ! csw->checkSignature())
			throw std::runtime_error("Test HiDi failed, bad CSW signature\n");
		if (csw->getTag() != tag)
			throw std::runtime_error("Bad tag response\n");
		if (csw->getStatus() != 0)
			throw std::runtime_error("CSW with non-zero status");
	}
	catch(std::exception &e)
	{
		std::cout << "\x1B[1;31m" << e.what() << "\x1B[0m" << std::endl;
		usbdev->resetRecovery();
		usbdev->reset();
		return(1);
	}
	std::cout << "\x1B[1;32m" << "Test Hi=Di success"
	          << "\x1B[0m" << std::endl;

	return(0);
}

int test_HoDo(UsbIf *usbdev)
{
	Cbw *c;
	Csw *csw;
	uint32_t tag = 0xBABE0012;
	uint8_t buffer[1024];
	int result;
	int read_len;

	std::cout << std::endl << "\x1B[1;36m"
	          << "Test Ho = Do (case #12)"
	          << "\x1B[0m" << std::endl;

	try
	{
#ifdef VENDOR
		// Use a VENDOR command to test HoDo
		c = new Cbw(0x00);
		c->setTag(tag);
		c->setLength(512);
		c->setCB((uint8_t *)"\xC0\x00\x00\x00\x00\x00", 6);
#else
		// Use a WRITE(10) command to test Ho=Do
		c = new Cbw(0x00);
		c->setTag(tag);
		c->setLength(512);
		c->setCB((uint8_t *)"\x2A\x00\x00\x00\x00\x00\x00\x00\x01\x00", 10);
#endif
		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write CBW failed");

		memset(buffer, 0, 512);
		result = usbdev->write(buffer, 512);
		if (result == -9)
			std::cout << " - STALL during data phase (ok, why not)" << std::endl;
		else if (result)
			throw std::runtime_error("Write Data failed");

		// Now, try to read CSW
		read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("Read CSW failed");
		csw = new Csw(buffer, read_len);

		if ( ! csw->checkSignature())
			throw std::runtime_error("Test Ho=Do failed, bad CSW signature\n");
		if (csw->getTag() != tag)
			throw std::runtime_error("Bad tag response\n");
		if (csw->getStatus() != 0)
			throw std::runtime_error("CSW with non-zero status");
	}
	catch(std::exception &e)
	{
		std::cout << "\x1B[1;31m" << e.what() << "\x1B[0m" << std::endl;
		recovery(usbdev);
		return(1);
	}
	std::cout << "\x1B[1;32m" << "Test Ho=Do success"
	          << "\x1B[0m" << std::endl;

	return(0);
}

int test_recovery(UsbIf *usbdev)
{
	uint32_t tag = 0xBABEDEAD;
	int result;

	std::cout << "Test Recovery ...";

	try
	{
		// Use a TEST UNIT READY command to test device
		Cbw *c = new Cbw(0x80);
		c->setTag(tag);
		c->setCB((uint8_t *)"\x00\x00\x00\x00\x00\x00", 6);

		result = usbdev->write(c->buffer(), 31);
		if (result)
			throw std::runtime_error("Write failed");

		uint8_t buffer[1024];
		int read_len = 1024;
		result = usbdev->read(buffer, &read_len);
		if (result < 0)
			throw std::runtime_error("read CSW");
		Csw csw(buffer, read_len);

		if ( ! csw.checkSignature())
			throw std::runtime_error("bad CSW signature");
		if (csw.getTag() != tag)
			throw std::runtime_error("bad tag response");
		if (csw.getStatus() == 2)
			throw std::runtime_error("Device report a 0x02 status");
	}
	catch(std::exception &e)
	{
		std::cout << "failed, " << e.what() << std::endl;
		return(-1);
	}

	return(0);
}
/* EOF */
