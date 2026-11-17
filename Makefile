CC = gcc
CFLAGS = -O3 -Wall -Wextra -D__USE_MINGW_ANSI_STDIO=1 -Iinclude
CORE_SRC = src/huffman.c src/bitio.c src/crc32.c

all: huffman.exe huffman_gui.exe

huffman.exe: src/main.c $(CORE_SRC)
	$(CC) $(CFLAGS) src/main.c $(CORE_SRC) -o huffman.exe

huffman_gui.exe: src/gui.c $(CORE_SRC)
	$(CC) $(CFLAGS) src/gui.c $(CORE_SRC) -o huffman_gui.exe -mwindows -lcomctl32 -lcomdlg32 -lshell32

clean:
	rm -f huffman.exe huffman_gui.exe test_runner.exe *.huf *.restored tests/*.huf tests/*.restored

.PHONY: all clean
