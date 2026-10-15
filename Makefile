CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 $(shell pkg-config --cflags sdl2 fluidsynth)
LDFLAGS = $(shell pkg-config --libs sdl2 fluidsynth) -lm
SRC = src/main.c src/assets.c src/game.c src/audio.c
BIN = dxball

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC) src/dxball.h src/scroller.inc
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

run: $(BIN)
	./$(BIN) original/Dxball
