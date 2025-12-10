.RECIPEPREFIX := >
CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c17 -O2
LDFLAGS=-lpthread

GTK_PACKAGES=gtk+-3.0 webkit2gtk-4.1
GTK_CFLAGS=$(shell pkg-config --cflags $(GTK_PACKAGES))
GTK_LIBS=$(shell pkg-config --libs $(GTK_PACKAGES))

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=lkj

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
>$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $@ $^ $(LDFLAGS) $(GTK_LIBS)

%.o: %.c
>$(CC) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

run: $(BIN)
>./$(BIN)

clean:
>rm -f $(OBJ) $(BIN)
