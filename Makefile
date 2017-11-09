LDFLAGS ?= -ludev
CFLAGS = -O3 -pedantic -Wall
CC ?= $(CROSSCOMPILE)-gcc

ifneq ($(OS),Windows_NT)
	CFLAGS += -fPIC

	ifeq ($(shell uname),Darwin)
		LDFLAGS += -dynamiclib -undefined dynamic_lookup
	endif
endif

ifeq ($(ERL_INCLUDE_DIR),)
ERL_ROOT_DIR = $(shell erl -eval "io:format(\"~s~n\", [code:root_dir()])" -s init stop -noshell)
ifeq ($(ERL_ROOT_DIR),)
   $(error Could not find Erlang installation.)
endif
ERL_INCLUDE_DIR = "$(ERL_ROOT_DIR)/usr/include"
endif

ERL_CFLAGS ?= -I$(ERL_INCLUDE_DIR)

.PHONY: all clean

all: clean priv priv/udev.so

priv:
	mkdir -p priv

priv/udev.so: src/udev.c
	$(CC) $^ $(ERL_CFLAGS) $(LDFLAGS) $(CFLAGS) -shared -o $@

clean:
	$(RM) priv/udev.so
