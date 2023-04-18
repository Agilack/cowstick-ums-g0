##
 # @file  usb_msc/Makefile
 # @brief Script to compile USB MSC test
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
TARGET=usb_msc
CPP=g++
CFLAGS  = -Wall -Wextra
CFLAGS += -g
LDFLAGS = $(CFLAGS) -lusb-1.0

SRC  = main.cpp params.cpp
SRC += usb_if.cpp cbw.cpp csw.cpp

COBJ = $(patsubst %.cpp,%.o,$(SRC))

all: $(COBJ)
	$(CPP) $(LDFLAGS) -o $(TARGET) $(COBJ)

clean:
	rm -f $(TARGET) *.o
	rm -f *~

%.o: %.cpp
	g++ $(CFLAGS) -c $< -o $@
