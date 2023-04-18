/**
 * @file  usb_msc/params.cpp
 * @brief The Params class is used to manage configuration and parameters
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
#include <cstring>
#include <cstdio>
#include "params.hpp"

Params * Params::mInstance = 0;

Params::Params()
{
	mVid = 0x3608; // Default VID: Agilack
	mPid = 0xC720; // Default PID: Cowstick-ums r1
}

Params::~Params()
{
}

/**
 * @brief Get access to the Params instance
 *
 * The Params class works with a single object into the application. To allow
 * only one instance (singleton) object constructor is private. This method is
 * used to get access to this static, unique variable.
 */
Params *Params::getInstance()
{
	if ( ! mInstance)
		mInstance = new Params;

	return mInstance;
}

uint16_t Params::getPid()
{
	return( getInstance()->mPid );
}

uint16_t Params::getVid()
{
	return( getInstance()->mVid );
}

void Params::helpUsage(char *name)
{
	printf(" - A test utility for USB-MSC interfaces - \n");
	printf("Usage: %s <options>\n", name);
	printf("  --help     : Show command line help (this message)\n");
	printf("  -d vid:pid : Specify vendor-id and product-id of usb device to test (in hex)\n");
}

int Params::loadCmdline(int argc, char **argv)
{
	int index;

	for (index = 1; index < argc; index++)
	{
		if ((strcmp(argv[index], "--help") == 0) ||
		    (strcmp(argv[index], "/?")     == 0) )
		{
			helpUsage(argv[0]);
		}
		else if (strcmp(argv[index], "-d") == 0)
		{
			uint16_t id;
			char *sep, *arg;
			int i;

			/* Move index to next argument to decode vid:pid */
			index++;
			if (index == argc)
			{
				printf("Missing \"vid:pid\" argument to \"-d\"\n\n");
				helpUsage(argv[0]);
				return(-1);
			}
			/* Search vid/pid separator */
			sep = strchr(argv[index], ':');
			if (sep == 0)
			{
				printf("Malformed \"vid:pid\" argument (missing ':' separator)\n\n");
				helpUsage(argv[0]);
				return(-1);
			}
			*sep = 0;
			arg = argv[index];
			id = 0;
			for (i = 0; i < 4; i++)
			{
				if (arg[i] == 0)
					break;
				id = (id << 4);
				if ((arg[i] >= '0') && (arg[i] <= '9'))
					id |= ((arg[i] - '0') & 0x0F);
				else if ((arg[i] >= 'a') && (arg[i] <= 'f'))
					id |= 10 + ((arg[i] - 'a') & 0x0F);
				else if ((arg[i] >= 'A') && (arg[i] <= 'F'))
					id |= 10 + ((arg[i] - 'A') & 0x0F);
				else
				{
					printf("Malformed \"vid:pid\" argument (not an hex value)\n\n");
					return(-1);
				}
			}
			getInstance()->mVid = id;
			// Decode PID argument
			arg = (sep + 1);
			id = 0;
			for (i = 0; i < 4; i++)
			{
				if (arg[i] == 0)
					break;
				id = (id << 4);
				if ((arg[i] >= '0') && (arg[i] <= '9'))
					id |= ((arg[i] - '0') & 0x0F);
				else if ((arg[i] >= 'a') && (arg[i] <= 'f'))
					id |= 10 + ((arg[i] - 'a') & 0x0F);
				else if ((arg[i] >= 'A') && (arg[i] <= 'F'))
					id |= 10 + ((arg[i] - 'A') & 0x0F);
				else
				{
					printf("Malformed \"vid:pid\" argument (not an hex value)\n\n");
					return(-1);
				}
			}
			getInstance()->mPid = id;
		}
		else
		{
			printf("Unknown command line argument \"%s\"\n\n",argv[index]);
			helpUsage(argv[0]);
		}
	}
	return(0);
}
/* EOF */
