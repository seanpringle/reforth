CFLAGS?=-Wall -Wno-unused -Wno-unused-result -O2 -g
TURNKEY={ echo 'const char src_turnkey[] = {'; cat $(1) | xxd -i; echo ',0};'; } >src_turnkey.c

normal: generic shell editor tools cgi

generic:
	{ echo 'const char src_base[] = {'; cat base.fs | xxd -i; echo ',0};'; } >src_base.c
	$(CC) -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth reforth.c $(CFLAGS)
	$(CC) -DDEBUG -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o reforth_debug reforth.c $(CFLAGS)
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

cgi:
	$(call TURNKEY,web.fs)
	$(CC) -DTURNKEY -DLIB_SHELL -DLIB_REGEX -DLIB_FORK -o web reforth.c $(CFLAGS)
	strip web

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
