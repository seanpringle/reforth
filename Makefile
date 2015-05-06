CFLAGS?=-Wall -Wno-unused -Wno-unused-result -O2 -g

normal: compress generic shell editor

generic:
	./fstoc base.fs
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth reforth.c $(CFLAGS) -lmysqlclient
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth_debug reforth.c $(CFLAGS) -lmysqlclient
	objdump -d reforth >reforth.dump

shell:
	./fstoc base.fs --turnkey shell.fs
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o rf reforth.c $(CFLAGS)

editor:
	./fstoc base.fs --turnkey editor.fs
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o re reforth.c $(CFLAGS)

compress:
	$(CC) -o fstoc fstoc.c $(CFLAGS)

compare:
	gcc   -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_gcc reforth.c $(CFLAGS)
	objdump -d reforth_gcc >reforth_gcc.dump
	clang -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_clang reforth.c $(CFLAGS)
	objdump -d reforth_clang >reforth_clang.dump

bench:
	./fstoc base.fs --turnkey test.fs
	gcc   -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_gcc   reforth.c $(CFLAGS)
	clang -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_clang reforth.c $(CFLAGS)
	bash -c "time ./test_gcc"
	bash -c "time ./test_clang"

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
