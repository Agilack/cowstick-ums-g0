/**
 * @file  tests/usb_bulk/log.c
 * @brief Log messages and/or events to console
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
