CFLAGS?=-Wall -O3 -g

normal:
	$(CC) -o fstoc fstoc.c $(CFLAGS)
	./fstoc base.fs

	$(CC) -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth reforth.c
	objdump -d reforth >reforth.dump

	clang -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_clang reforth.c
	objdump -d reforth_clang >reforth_clang.dump

	gcc -DLIB_REGEX -DLIB_FORK $(CFLAGS) -o reforth_gcc reforth.c
	objdump -d reforth_gcc >reforth_gcc.dump

bench:
	sh -c "time ./reforth_gcc test.fs"
	sh -c "time ./reforth_clang test.fs"

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
