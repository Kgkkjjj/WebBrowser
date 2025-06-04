CC ?= gcc
CFLAGS += `pkg-config --cflags gtk+-3.0 webkit2gtk-4.1`
LDFLAGS += `pkg-config --libs gtk+-3.0 webkit2gtk-4.1`

SRC = src/main.c
OUT = browser

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)
