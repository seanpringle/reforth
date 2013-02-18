CFLAGS?=-Wall -Wno-unused -O2 -g -std=c99

all: clean parser editor tetris

parser:
	$(CC) $(CFLAGS) -o reforth reforth.c

editor:
	./reforth editor.fs >editor.c
	$(CC) $(CFLAGS) -o editor editor.c

tetris:
	./reforth tetris.fs >tetris.c
	$(CC) $(CFLAGS) -o tetris tetris.c

bench:
	./reforth test.fs >test.c
	gcc $(CFLAGS) -o test test.c
	sh -c "time ./test"
	clang $(CFLAGS) -o test test.c
	sh -c "time ./test"

clean:
	rm -f reforth editor tetris test
