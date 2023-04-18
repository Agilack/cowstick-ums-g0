/**
 * @file  usb_msc/csw.cpp
 * @brief Implementation of CSW packets (MSC response)
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
#include "csw.hpp"

typedef struct __attribute__((packed))
{
	uint32_t signature;
	uint32_t tag;
	uint32_t residue;
	uint8_t  status;
} csw_packet;

Csw::Csw()
{
}

Csw::Csw(uint8_t *buffer, unsigned int len)
{
	csw_packet *pkt = 0;
	unsigned int i;

	if (len != 13)
		goto err_dump;

	pkt = (csw_packet *)buffer;
	mSignature = pkt->signature;
	mTag       = pkt->tag;
	mResidue   = pkt->residue;
	mStatus    = pkt->status;
	printf("Signature %.8X\n", pkt->signature);
	printf("Tag       %.8X\n", pkt->tag);
	printf("Residue   %d\n",   pkt->residue);
	printf("status    %.2X\n", pkt->status);
	return;

err_dump:
	for (i = 0; i < len; i++)
	{
		printf("%.2X", buffer[i]);
		if ((i % 16) == 15)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");
}

Csw::~Csw()
{
}

bool Csw::checkSignature()
{
	if (mSignature == 0x53425355)
		return true;

	return false;
}

uint32_t Csw::getTag()
{
	return mTag;
}

uint32_t Csw::getResidue()
{
	return mResidue;
}

uint8_t Csw::getStatus()
{
	return mStatus;
}
/* EOF */
