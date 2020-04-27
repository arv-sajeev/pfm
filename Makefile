# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# binary name
APP = cuup.exe

# all source are stored in SRCS-y
SRCS-y :=	cuup.c \
		pfm.c \
		pfm_log.c \
		pfm_worker_loop.c \
		pfm_link.c \
		pfm_rx_loop.c \
		pfm_tx_loop.c \
		pfm_kni.c \
		pfm_ring.c \
		pfm_classifier.c \
		pfm_dist_loop.c	\
		pfm_cli.c	\
		pfm_route.c

# Build using pkg-config variables if possible
ifeq ($(shell pkg-config --exists libdpdk && echo 0),0)

all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)

PKGCONF ?= pkg-config

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = -Wl,-Bstatic $(shell $(PKGCONF) --static --libs libdpdk)

build/$(APP)-shared: $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP) build/$(APP)-static build/$(APP)-shared
	test -d build && rmdir -p build || true

else # Build using legacy build system

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
$(error This application can only operate in a linux environment, \
please change the definition of the RTE_TARGET environment variable)
endif

#CFLAGS += -O3 -g -DTRACE  -DCONSLOG
CFLAGS += -O3 -g -DTRACE  
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.extapp.mk
endif
