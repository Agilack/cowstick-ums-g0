/**
 * @file  usb_msc/params.hpp
 * @brief Definitions for the parameters class
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
#ifndef PARAMS_HPP
#define PARAMS_HPP
#include <cstdint>

using namespace std;

class Params
{
public:
	~Params();
	static Params* getInstance();
	static uint16_t getVid();
	static uint16_t getPid();
	static void helpUsage(char *name);
	static int  loadCmdline(int argc, char **argv);

private:
	Params();
private:
	static Params *mInstance;

private:
	uint16_t mVid;
	uint16_t mPid;
};

#endif
/* EOF */
