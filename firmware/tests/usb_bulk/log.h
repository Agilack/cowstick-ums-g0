/**
 * @file  tests/usb_bulk/log.h
 * @brief Headers and definitions for the log messages functions
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
#ifndef LOG_H
#define LOG_H

void log_fail(void);
void log_success(void);
void log_title(char *str);

#endif
/* EOF */
