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
#ifndef MSC_CBW_HPP
#define MSC_CBW_HPP
#include <cstdint>

using namespace std;

class Cbw
{
public:
	Cbw(uint8_t flags = 0, uint32_t len = 0);
	~Cbw();

	uint8_t *buffer();
	void dump();
	int  length();

	int  getFlags();
	void setCB(uint8_t *block, unsigned int len);
	void setLength(uint32_t len);
	void setLun(uint8_t  lun);
	void setTag(uint32_t tag);

private:
	unsigned int mLun;
	unsigned int mCbLength;
	uint8_t  mBuffer[31];
};

#endif
/* EOF */
