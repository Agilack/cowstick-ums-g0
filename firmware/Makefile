##
 # @file  Makefile
 # @brief Script to compile sources and create flat binary using "make" program
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
TARGET   ?= firmware
CROSS    ?= arm-none-eabi-
BUILDDIR ?= build

SRC  = main.c hardware.c log.c uart.c spi.c time.c usb.c
SRC += scsi.c usb_msc.c
SRC += libc.c mem.c
ASRC = startup.s libasm.s

CC = $(CROSS)gcc
OC = $(CROSS)objcopy
OD = $(CROSS)objdump
GDB = $(CROSS)gdb
OCD = openocd

CFLAGS  = -mcpu=cortex-m0plus -mthumb
CFLAGS += -nostdlib -Os -ffunction-sections
CFLAGS += -fno-builtin-memset -fno-builtin-memcpy
CFLAGS += -Wall -Wextra -Wconversion -pedantic
CFLAGS += -g -DUART_FIFO_SW
#CFLAGS += -DUSB_DEBUG

LDFLAGS = -nostartfiles -T src/cowstick-ums.ld -Wl,-Map=$(TARGET).map,--cref,--gc-sections -static

COBJ = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))
AOBJ = $(patsubst %.s, $(BUILDDIR)/%.o, $(ASRC))

## Directives ##################################################################

all: $(BUILDDIR) $(AOBJ) $(COBJ)
	@echo "  [LD] $(TARGET)"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET).elf $(AOBJ) $(COBJ)
	@echo "  [OC] $(TARGET).bin"
	@$(OC) -S $(TARGET).elf -O binary $(TARGET).bin
	@echo "  [OD] $(TARGET).dis"
	@$(OD) -D $(TARGET).elf > $(TARGET).dis

clean:
	@echo "  [RM] $(TARGET).*"
	@rm -f $(TARGET).elf $(TARGET).map $(TARGET).bin $(TARGET).dis
	@echo "  [RM] Temporary object (*.o)"
	@rm -f $(BUILDDIR)/*.o
	@echo "  [RM] Clean editor temporary files (*~) "
	@find -name "*~" -exec rm -f {} \;

$(BUILDDIR):
	@echo "  [MKDIR] $@"
	@mkdir -p $(BUILDDIR)

$(AOBJ) : $(BUILDDIR)/%.o : src/%.s
	@echo "  [AS] $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(COBJ) : $(BUILDDIR)/%.o: src/%.c
	@echo "  [CC] $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).bin:
	@echo ""
	@echo "firmware.bin now available, please compile firmware first"
	@echo ""
	@false

openocd:
	$(OCD) -f scripts/openocd.cfg

debug:
	$(GDB) --command=scripts/gdb.cfg $(TARGET).elf

load: $(TARGET).bin
	$(OCD) -f scripts/openocd.cfg -c "mem_load ./$(TARGET).bin"
reboot: $(TARGET).bin
	$(OCD) -f scripts/openocd.cfg -c "mcu_reboot"
