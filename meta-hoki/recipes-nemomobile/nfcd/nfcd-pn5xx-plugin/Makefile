CC ?= $(CROSS_COMPILE)gcc
STRIP ?= $(CROSS_COMPILE)strip

PKGS = glib-2.0 libglibutil libncicore libnciplugin nfcd-plugin

CFLAGS += -Wall -Wextra -Wno-unused-parameter -fPIC -shared -DNFC_PLUGIN_EXTERNAL
CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS))
LDFLAGS += -lpthread

PLUGIN_DIR ?= /usr/lib/nfcd/plugins

SRCS = src/pn5xx_plugin.c src/pn5xx_adapter.c
TARGET = pn5xx.so

all: $(TARGET)

$(TARGET): $(SRCS) src/pn5xx_adapter.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PLUGIN_DIR)
	install -m 0644 $(TARGET) $(DESTDIR)$(PLUGIN_DIR)/

clean:
	rm -f $(TARGET)

.PHONY: all install clean
