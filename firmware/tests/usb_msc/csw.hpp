/**
 * @file  usb_msc/csw.hpp
 * @brief The Csw class is used to manage CSW packets (MSC responses)
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
#ifndef CSW_HPP
#define CSW_HPP
#include <cstdint>

using namespace std;

class Csw
{
public:
	Csw(uint8_t *buffer, unsigned int len);
	Csw();
	~Csw();

	bool checkSignature();
	uint32_t getTag();
	uint32_t getResidue();
	uint8_t getStatus();

private:
	uint32_t mSignature;
	uint32_t mTag;
	uint32_t mResidue;
	uint8_t  mStatus;
};
#endif
/* EOF */
