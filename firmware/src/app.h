/**
 * @file  app.h
 * @brief Definitions and prototypes for custom application interface
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
#ifndef APP_H
#define APP_H

void app_init(void);

extern void (*app_periodic)(void);
extern void (*app_reset)(void);

#endif
/* EOF */
