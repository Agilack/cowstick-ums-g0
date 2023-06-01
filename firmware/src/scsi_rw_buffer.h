/**
 * @file  scsi_rw_buffer.h
 * @brief Headers and definitions for SCSI READ_BUFFER and WRITE_BUFFER
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
#ifndef SCSI_RW_BUFFER_H
#define SCSI_RW_BUFFER_H
#include "scsi.h"
#include "types.h"

#ifdef SCSI_USE_RW_BUFFER
int cmd10_read_buffer (lun *lun, scsi_context *ctx);
int cmd10_write_buffer(lun *lun, scsi_context *ctx);
#endif

#endif
