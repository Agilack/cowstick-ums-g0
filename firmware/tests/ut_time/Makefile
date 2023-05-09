##
 # @file  tests/ut_time/Makefile
 # @brief Script to compile time unit-test
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
TARGET=ut_time
CFLAGS = -I. -I../../src -g

all:
	cc $(CFLAGS) -o main.o -c main.c
	cc $(CFLAGS) -o time.o -c ../../src/time.c
	cc $(CFLAGS) -o $(TARGET) main.o time.o

clean:
	rm -f $(TARGET) *.o
	rm -f *~
