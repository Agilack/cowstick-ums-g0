/**
 * @file  usb_msc/msc_cbw.hpp
 * @brief The Cbw class is used to manage CBW packets (requests)
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
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "cbw.hpp"

typedef struct __attribute__((packed))
{
	uint32_t signature;
	uint32_t tag;
	uint32_t data_length;
	uint8_t  flags;
	uint8_t  lun;
	uint8_t  cb_length;
	uint8_t  cb[16];
} cbw_packet;

Cbw::Cbw(uint8_t flags, uint32_t len)
{
	mCbLength = 0;

	cbw_packet *pkt;
	/* Clear buffer */
	memset(mBuffer, 0, 31);
	/* Initialize packet */
	pkt = (cbw_packet *)mBuffer;
	pkt->signature   = 0x43425355;
	pkt->flags       = flags;
	pkt->data_length = len;
}

Cbw::~Cbw()
{
}

uint8_t *Cbw::buffer()
{
	return mBuffer;
}

void Cbw::dump()
{
	int i;

	for (i = 0; i < 31; i++)
	{
		printf("%.2X", mBuffer[i]);
		if ((i % 16) == 15)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");
}

int Cbw::getFlags()
{
	cbw_packet *pkt = (cbw_packet *)mBuffer;
	return pkt->flags;
}

int Cbw::length()
{
	int len = 0;

	// Compute length of the packet, minus CB buffer
	len = sizeof(cbw_packet) - 16;
	// Add length of current CB payload
	len += mCbLength;

	return(len);
}

void Cbw::setCB(uint8_t *block, unsigned int len)
{
	cbw_packet *pkt;

	if (len > 16)
		throw std::runtime_error("CBW invalid length");

	mCbLength = len;

	pkt = (cbw_packet *)mBuffer;
	memcpy(pkt->cb, block, mCbLength);
	pkt->cb_length = (mCbLength & 0x1F);
}

void Cbw::setLength(uint32_t len)
{
	cbw_packet *pkt = (cbw_packet *)mBuffer;
	pkt->data_length = len;
}

void Cbw::setLun(uint8_t lun)
{
	cbw_packet *pkt = (cbw_packet *)mBuffer;
	pkt->lun = (lun & 0x0F);
}

void Cbw::setTag(uint32_t tag)
{
	cbw_packet *pkt = (cbw_packet *)mBuffer;
	pkt->tag = tag;
}
/* EOF */
