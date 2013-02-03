CFLAGS?=-Wall -O2 -g

normal: compress generic shell editor

generic:
	./fstoc base.fs
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL $(CFLAGS) -lmysqlclient -o reforth reforth.c
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL $(CFLAGS) -lmysqlclient -o reforth_debug reforth.c
	objdump -d reforth >reforth.dump

shell:
	./fstoc base.fs --turnkey shell.fs
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o rf reforth.c

editor:
	./fstoc base.fs --turnkey editor.fs
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o re reforth.c

compress:
	$(CC) -o fstoc fstoc.c $(CFLAGS)

compare:
	gcc -DLIB_SHELL -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_gcc reforth.c
	objdump -d reforth_gcc >reforth_gcc.dump
	clang -DLIB_SHELL -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_clang reforth.c
	objdump -d reforth_clang >reforth_clang.dump

bench:
	./parse test.fs >test.c
	$(CC) -O1 -o test1 test.c
	$(CC) -O2 -o test2 test.c
	$(CC) -O3 -o test3 test.c
	sh -c "time ./test1"
	sh -c "time ./test2"
	sh -c "time ./test3"

parser:
	$(CC) $(CFLAGS) -o parse parse.c
	./parse ed.fs >ed.c
	$(CC) $(CFLAGS) -Wno-unused -o ed ed.c
	objdump -d ed >ed.dump
	#valgrind ./parse ptest.fs >ptest.c
	#$(CC) $(CFLAGS) -Wno-unused -o ptest ptest.c
	#objdump -d ptest >ptest.dump

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
