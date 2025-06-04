CC ?= gcc
# Detect the available WebKit2GTK version (4.1 or 4.0)
WEBKIT_PKG := $(shell pkg-config --exists webkit2gtk-4.1 && echo webkit2gtk-4.1 || echo webkit2gtk-4.0)
CFLAGS += $(shell pkg-config --cflags gtk+-3.0 $(WEBKIT_PKG))
LDFLAGS += $(shell pkg-config --libs gtk+-3.0 $(WEBKIT_PKG))

SRC = src/main.c
OUT = browser

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)
