CFLAGS?=-Wall -O3 -g

normal:
	$(CC) -o fstoc fstoc.c $(CFLAGS)
	./fstoc base.fs
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL $(CFLAGS) -lmysqlclient -o reforth reforth.c
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL $(CFLAGS) -lmysqlclient -o reforth_debug reforth.c
	objdump -d reforth >reforth.dump

compare:
	gcc -DLIB_SHELL -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_gcc reforth.c
	objdump -d reforth_gcc >reforth_gcc.dump
	clang -DLIB_SHELL -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_clang reforth.c
	objdump -d reforth_clang >reforth_clang.dump

bench:
	sh -c "time ./reforth_gcc test.fs 2>stderr_gcc"
	sh -c "time ./reforth_clang test.fs 2>stderr_clang"

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
