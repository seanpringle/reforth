CFLAGS?=-Wall -Wno-unused -O2 -g

all: parser editor

parser:
	$(CC) $(CFLAGS) -o reforth reforth.c

editor:
	./reforth editor.fs >editor.c
	$(CC) $(CFLAGS) -o editor editor.c

clean:
	rm reforth editor
