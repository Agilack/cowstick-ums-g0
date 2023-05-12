/**
 * @file  api.s
 * @brief Declaration of API entries tables
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

.section .api
.align 4

api_index:
	.long 0 /* Reserved for system functions */
	.long libc_tbl
	.long time_tbl
	.long log_tbl

	.long 0 /* Reserved for  generic USB functions */
	.long 0 /* Reserved for USB mass storage */
	.long 0 /* Reserved for SCSI */
	.long 0 

	.long 0
	.long 0 /* Rfu Flash */
	.long 0 /* Rfu Security functions */
	.long 0xDEADBEEF

/* ---------------------------------- */
/* --   Sub-tables per functions   -- */
/* ---------------------------------- */

libc_tbl:
	.long memcpy
	.long memset
	.long strcat
	.long strchr
	.long 0 // Reserved for strcmp
	.long strcpy
	.long strlen
	.long strncat
	.long 0 // Reserved for strnchr
	.long strncmp
	.long strncpy
	.long atoi
	.long itoa
	.long 0 // Rfu    
	.long 0 // Rfu
	.long 0 // Rfu

/* Table of time functions */
time_tbl:
	.long time_now
	.long time_since
	.long time_diff_ms
	.long 0

/* Table of log functions */
log_tbl:
	.long 0 // Reserved for log configure (set level)
	.long 0 // Rfu
	.long log_putc
	.long log_puts
	.long log_putdec
	.long log_puthex
	.long log_dump
	.long log_print
