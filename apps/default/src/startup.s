/**
 * @file  startup.s
 * @brief Cowstick-UMS application header with vector table
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
.syntax unified
.code 16

.section .app_vector
.align 4

.globl app_vector
app_vector:
	/* Basic app vectors */
	.long app_init      // Called on startup
	.long app_periodic  // Called on every main loop cycle
	.long app_reset     // Called when device is reset
	.long 0xBABEFACE    // Signature

	/* Extended vectors (rfu) */
	.long 0
	.long 0
	.long 0
	.long 0

/* EOF */
