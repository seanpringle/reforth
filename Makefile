CFLAGS?=-Wall -Wno-unused -Wno-unused-result -O2 -g
TURNKEY={ echo 'const char src_turnkey[] = {'; cat $(1) | xxd -i; echo ',0};'; } >src_turnkey.c

normal: generic shell editor tools

generic:
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth reforth.c $(CFLAGS) -lmysqlclient
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -DLIB_MYSQL -o reforth_debug reforth.c $(CFLAGS) -lmysqlclient
	objdump -d reforth >reforth.dump
	strip reforth

shell:
	$(call TURNKEY,shell.fs)
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o rf reforth.c $(CFLAGS)
	strip rf

editor:
	$(call TURNKEY,editor.fs)
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o re reforth.c $(CFLAGS)
	strip re

tools:
	$(call TURNKEY,gmenu.fs)
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o gmenu reforth.c $(CFLAGS)
	strip gmenu

compare:
	gcc   -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_gcc reforth.c $(CFLAGS)
	objdump -d reforth_gcc >reforth_gcc.dump
	clang -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_clang reforth.c $(CFLAGS)
	objdump -d reforth_clang >reforth_clang.dump

bench:
	$(call TURNKEY,test.fs)
	gcc   -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_gcc   reforth.c $(CFLAGS)
	clang -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o test_clang reforth.c $(CFLAGS)
	bash -c "time ./test_gcc"
	bash -c "time ./test_clang"

test:
	valgrind ./reforth

clean:
	rm reforth reforth_gcc reforth_clang
