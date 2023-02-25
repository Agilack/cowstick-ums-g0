##
 # @file  tests/usb_device/Makefile
 # @brief Script to compile usb device unit-test
 #
 # @author Saint-Genest Gwenael <gwen@cowlab.fr>
 # @copyright Agilack (c) 2023
 #
 # @page License
 # Cowstick-UMS firmware is free software: you can redistribute it and/or
 # modify it under the terms of the GNU Lesser General Public License
 # version 3 as published by the Free Software Foundation. You should have
 # received a copy of the GNU Lesser General Public License along with this
 # program, see LICENSE.md file for more details.
 # This program is distributed WITHOUT ANY WARRANTY.
##
TARGET=ut_device
CC=gcc
CFLAGS  = -Wall -Wextra
CFLAGS += -g
LDFLAGS = $(CFLAGS) -lusb-1.0

all: main.o
	$(CC) $(LDFLAGS) -o $(TARGET) main.o

clean:
	rm -f $(TARGET) *.o
	rm -f *~
