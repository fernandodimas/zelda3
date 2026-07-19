.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR      ?= $(CURDIR)
DEVKITA64   := $(DEVKITPRO)/devkitA64
TOOLSBIN    := $(DEVKITPRO)/tools/bin
PORTLIBS    := $(DEVKITPRO)/portlibs/switch
LIBNX       := $(DEVKITPRO)/libnx

CC          := $(DEVKITA64)/bin/aarch64-none-elf-gcc
CXX         := $(DEVKITA64)/bin/aarch64-none-elf-g++
LD          := $(CXX)
elf2nro     := $(TOOLSBIN)/elf2nro
nacptool    := $(TOOLSBIN)/nacptool

PROJECT_DIR := $(realpath $(TOPDIR)/../../..)
SRC_DIR     := $(PROJECT_DIR)/src
SNES_DIR    := $(PROJECT_DIR)/snes
TARGET      := zelda3
BUILD       := $(TOPDIR)/bin

CFILES       := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SNES_DIR)/*.c)
CFILES       += $(PROJECT_DIR)/third_party/gl_core/gl_core_3_1.c
CFILES       += $(PROJECT_DIR)/third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c
OFILES       := $(addprefix $(BUILD)/, $(notdir $(CFILES:.c=.o)))

APP_TITLE   := The Legend of Zelda: A Link to the Past
APP_AUTHOR  := snesrev & Lywx
APP_VERSION := $(shell git rev-parse --short HEAD 2>/dev/null || echo "custom") $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "dev")

ARCH        := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS      := -g -Wall -O2 -ffunction-sections -Wno-parentheses $(ARCH)
CFLAGS      += -D__SWITCH__ -DSTBI_NO_THREAD_LOCALS
CFLAGS      += -I$(PROJECT_DIR) -I$(SRC_DIR)
CFLAGS      += -I$(PORTLIBS)/include/SDL2 -I$(PORTLIBS)/include
CFLAGS      += -isystem $(LIBNX)/include
CFLAGS      += -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -ftls-model=local-exec
CFLAGS      += -std=gnu11
LDFLAGS     := -specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(BUILD)/zelda3.map
LDFLAGS     += -L$(PORTLIBS)/lib -L$(LIBNX)/lib -fPIE
LIBS        := $(PORTLIBS)/lib/libSDL2.a -lEGL -lglapi -ldrm_nouveau -lstdc++ -lnx -lm -lpthread

all: $(TOPDIR)/$(TARGET).nro

$(TOPDIR)/$(TARGET).nro: $(TOPDIR)/$(TARGET).elf $(TOPDIR)/$(TARGET).nacp
	@$(elf2nro) $< $@ --icon=$(PROJECT_DIR)/icon.png --nacp=$(TOPDIR)/$(TARGET).nacp
	@echo built ... $(notdir $@)

$(TOPDIR)/$(TARGET).nacp:
	@$(nacptool) --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@
	@echo built ... $(notdir $@)

$(TOPDIR)/$(TARGET).elf: $(OFILES)
	@echo linking $(notdir $@)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/zelda3.map: $(TOPDIR)/$(TARGET).elf

$(BUILD)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD)
	@echo $(notdir $<)
	$(CC) $(CFLAGS) -MMD -MP -MF $(BUILD)/$*.d -c $< -o $@

$(BUILD)/%.o: $(SNES_DIR)/%.c
	@mkdir -p $(BUILD)
	@echo $(notdir $<)
	$(CC) $(CFLAGS) -MMD -MP -MF $(BUILD)/$*.d -c $< -o $@

$(BUILD)/gl_core_3_1.o: $(PROJECT_DIR)/third_party/gl_core/gl_core_3_1.c
	@mkdir -p $(BUILD)
	@echo $(notdir $<)
	$(CC) $(CFLAGS) -MMD -MP -MF $(BUILD)/gl_core_3_1.d -c $< -o $@

$(BUILD)/opus_decoder_amalgam.o: $(PROJECT_DIR)/third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c
	@mkdir -p $(BUILD)
	@echo $(notdir $<)
	$(CC) $(CFLAGS) -MMD -MP -MF $(BUILD)/opus_decoder_amalgam.d -c $< -o $@

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TOPDIR)/$(TARGET).nro $(TOPDIR)/$(TARGET).nacp $(TOPDIR)/$(TARGET).elf

-include $(wildcard $(BUILD)/*.d)
