# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

KDIR ?= /lib/modules/$(shell uname -r)/build
SRC  := $(CURDIR)/src
BUILD := $(CURDIR)/build

SRCS := $(wildcard $(SRC)/*.c)
HDRS := $(wildcard $(SRC)/*.h)

all: $(BUILD)/ima_rtmr.ko

$(BUILD)/ima_rtmr.ko: $(SRCS) $(HDRS) $(SRC)/Kbuild
	@mkdir -p $(BUILD)
	@ln -sf $(SRCS) $(HDRS) $(BUILD)/
	@cp $(SRC)/Kbuild $(BUILD)/Kbuild
	$(MAKE) -C $(KDIR) M=$(BUILD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(BUILD) clean 2>/dev/null || true
	@rm -f $(BUILD)/*.c $(BUILD)/*.h $(BUILD)/Kbuild

.PHONY: all clean

# Cross-build for a specific kernel source tree: make KDIR=/path/to/linux
