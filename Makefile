CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2 -ffunction-sections -fdata-sections
LDFLAGS ?= -Wl,--gc-sections -s
LDLIBS ?= -lm

CORE = src/mz.c src/main.c
SDL = src/mz.c src/mz_sdl.c src/main_sdl.c

all: bin/mz.exe bin/mz_sdl.exe

bin/mz.exe: $(CORE)
	$(CC) $(CFLAGS) $(CORE) -Iinclude -o bin/mz.exe $(LDFLAGS) $(LDLIBS)

bin/mz_sdl.exe: $(SDL)
	$(CC) $(CFLAGS) $(SDL) -DMZ_WITH_SDL -Iinclude -IC:/msys64/ucrt64/include -LC:/msys64/ucrt64/lib -o bin/mz_sdl.exe $(LDFLAGS) -lSDL3 $(LDLIBS)

clean:
	rm -f bin/mz.exe bin/mz_sdl.exe
