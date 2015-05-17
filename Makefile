CFLAGS?=-Wall -Wno-unused -Wno-unused-result -O2 -g

normal: compress generic shell editor tools

generic:
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth reforth.c $(CFLAGS) -lmysqlclient
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth_debug reforth.c $(CFLAGS) -lmysqlclient
	objdump -d reforth >reforth.dump
	strip reforth

shell:
	./cstr turnkey shell.fs > src_turnkey.c
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o rf reforth.c $(CFLAGS)
	strip rf

editor:
	./cstr turnkey editor.fs > src_turnkey.c
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o re reforth.c $(CFLAGS)
	strip re

tools:
	./cstr turnkey menu.fs > src_turnkey.c
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o rp reforth.c $(CFLAGS)
	strip rp

compress:
	$(CC) -o cstr cstr.c $(CFLAGS)
	./cstr base base.fs > src_base.c
	strip cstr

compare:
	gcc   -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_gcc reforth.c $(CFLAGS)
	objdump -d reforth_gcc >reforth_gcc.dump
	clang -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_clang reforth.c $(CFLAGS)
	objdump -d reforth_clang >reforth_clang.dump

bench:
	./cstr turnkey test.fs > src_turnkey.c
	gcc   -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_gcc   reforth.c $(CFLAGS)
	clang -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_clang reforth.c $(CFLAGS)
	bash -c "time ./test_gcc"
	bash -c "time ./test_clang"

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
