#
# Argon2 reference source code package - reference C implementations
#
# Copyright 2015
# Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
#
# You may use this work under the terms of a Creative Commons CC0 1.0
# License/Waiver or the Apache Public License 2.0, at your option. The terms of
# these licenses can be found at:
#
# - CC0 1.0 Universal : https://creativecommons.org/publicdomain/zero/1.0
# - Apache 2.0        : https://www.apache.org/licenses/LICENSE-2.0
#
# You should have received a copy of both of these licenses along with this
# software. If not, they may be obtained at the above URLs.
#

SRC = src/argon2.c src/core.c src/blake2/blake2b.c src/thread.c src/encoding.c
OBJ = $(SRC:.c=.o)

CFLAGS += -std=c89 -O3 -Wall -g -Iinclude -Isrc -I../blake2

ifeq ($(NO_THREADS), 1)
CFLAGS += -DARGON2_NO_THREADS
else
CFLAGS += -pthread
endif

CI_CFLAGS := $(CFLAGS) -Werror=declaration-after-statement -D_FORTIFY_SOURCE=2 \
				-Wextra -Wno-type-limits -Werror -coverage -DTEST_LARGE_RAM

OPTTARGET ?= native
OPTTEST := $(shell $(CC) -Iinclude -Isrc -march=$(OPTTARGET) src/opt.c -c \
			-o /dev/null 2>/dev/null; echo $$?)
# Detect compatible platform
ifneq ($(OPTTEST), 0)
$(info Building without optimizations)
	SRC += src/ref.c
else
$(info Building with optimizations for $(OPTTARGET))
	CFLAGS += -march=$(OPTTARGET)
	SRC += src/opt.c
endif

KERNEL_NAME := $(shell uname -s)

LIB_NAME = argon2
PC_NAME = lib$(LIB_NAME).pc
# PC_SRC = $(PC_NAME).in

ifeq ($(KERNEL_NAME), SunOS)
	CC := gcc
	CFLAGS += -D_REENTRANT
endif

ifeq ($(KERNEL_NAME), Linux)
ifeq ($(CC), clang)
	CI_CFLAGS += -fsanitize=address -fsanitize=undefined
endif
endif

LIB_ST := lib$(LIB_NAME).a

# Some systems don't provide an unprefixed ar when cross-compiling.
AR=ar

LIBRARIES = $(LIB_ST)
HEADERS = include/argon2.h

# relative paths for different OS
# ifeq ($(KERNEL_NAME), $(filter $(KERNEL_NAME),DragonFly FreeBSD))

# main target
.PHONY: all
all: $(LIBRARIES)

$(LIB_ST): 	$(OBJ)
		$(AR) rcs $@ $^

.PHONY: clean
clean:
		rm -f '$(LIB_ST)'
		rm -rf *.dSYM
		cd src/ && rm -f *.o
		cd src/blake2/ && rm -f *.o

