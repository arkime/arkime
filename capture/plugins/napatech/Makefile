# Makefile for Arkime Napatech reader plugin (reader-napatech.so)
# Requires: Napatech Link-Capture Software (libntapi-dev) installed

INCLUDE_PCAP  = $(shell pcap-config --cflags 2>/dev/null || pkg-config --cflags libpcap 2>/dev/null || echo -I/usr/include/pcap)

INCLUDE_OTHER = -I../.. -I../../thirdparty \
                $(shell pkg-config --cflags glib-2.0)

# Napatech NTAPI installation directory (default after package install)
NTDIR         ?= /opt/napatech3

# Output plugin destination (override with make install ARKIME_PLUGIN_DIR=...)
ARKIME_PLUGIN_DIR ?= /opt/arkime/plugins

SRCS  = reader-napatech.c
TARGET = reader-napatech.so

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRCS) ../../arkime.h ../../hash.h ../../dll.h
	$(CC) -pthread --shared \
	    -I$(NTDIR)/include \
	    $(INCLUDE_PCAP) \
	    $(INCLUDE_OTHER) \
	    -o $@ \
	    -g -O2 -Wall -Wextra \
	    -D_GNU_SOURCE -fPIC \
	    $(SRCS) \
	    -Wl,-rpath=$(NTDIR)/lib \
	    -L$(NTDIR)/lib \
	    -lntapi

install: $(TARGET)
	install -m 755 $(TARGET) $(ARKIME_PLUGIN_DIR)/$(TARGET)
	@echo "Installed $(TARGET) -> $(ARKIME_PLUGIN_DIR)/$(TARGET)"

clean:
	rm -f $(TARGET) *.o
