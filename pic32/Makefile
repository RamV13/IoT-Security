# 
# Nikita Ermoshkin (ne75)
# Eric Dai (emd88)
# 
# September 2017
#

XC32 = /Applications/microchip/xc32/v1.44/bin
LOADER = ./ipecmd.sh

CC = $(XC32)/xc32-gcc
BIN2HEX = $(XC32)/xc32-bin2hex

OBJDIR = ./build
TARGET = final

PROCESSOR = 32MX250F128B
LOADTOOL = PKOB

UTILS_DIR = ./utils
TFT_DIR = $(UTILS_DIR)/tft
PT_DIR = $(UTILS_DIR)/protothreads

INCLUDES = -I$(UTILS_DIR) -I$(PT_DIR) -I$(TFT_DIR)

CFLAGS = -g -x c -c -mprocessor=$(PROCESSOR) -no-legacy-libc -D_SUPPRESS_PLIB_WARNING=1 -D_DISABLE_OPENADC10_CONFIGPORT_WARNING
LFLAGS = -no-legacy-libc -mprocessor=$(PROCESSOR)
LOADFLAGS = -TP$(LOADTOOL) -P$(PROCESSOR) -OL -M 

.PHONY: all setup load clean

all: setup $(OBJDIR)/$(TARGET).hex load clean-output

setup:
	mkdir -p $(OBJDIR)

$(OBJDIR)/$(TARGET).hex: $(OBJDIR)/$(TARGET).elf
	$(BIN2HEX) $<

$(OBJDIR)/$(TARGET).elf: $(OBJDIR)/tft.o $(OBJDIR)/main.o
	$(CC) $(LFLAGS) -o $@ $^

$(OBJDIR)/main.o: main.c
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $<

$(OBJDIR)/tft.o: $(TFT_DIR)/tft_unity.c
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $<

load: $(OBJDIR)/$(TARGET).hex
	$(LOADER) $(LOADFLAGS) -F$<

clean:
	rm -rf $(OBJDIR)/*
	rm *.xml* && rm log.*
