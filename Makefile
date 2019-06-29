PROJECT=toy-guitar
BUILD_DIR=bin

OPT = -O0
LDFLAGS += -Xlinker -Map=$(PROJECT).map
CFLAGS += -ggdb3
#CFLAGS += -flto
#CFLAGS += -funroll-loops
#LDFLAGS += -flto

INCLUDES += -Isrc -Iaudio

DEVICE=stm32l432kcu6u

OPENCM3_DIR=libopencm3

CFILES += src/main.c
CFILES += src/hal.c

src/audio.h: $(wildcard audio/*.wav)
	./generate_audio.sh $@ $^

bin/src/main.o: src/audio.h

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

.PHONY: gdb load loadquit

gdb: $(PROJECT).elf
	$(PREFIX)gdb $(PROJECT).elf -x init.gdb
load: $(PROJECT).elf
	$(PREFIX)gdb $(PROJECT).elf -x init.gdb -ex load
#loadquit: $(PROJECT).elf
#	$(PREFIX)gdb $(PROJECT).elf -batch -x init.gdb -ex load -ex kill -ex quit
stflash: $(PROJECT).bin
	st-flash write $(PROJECT).bin 0x08000000

.DEFAULT: all
