/*

MIT/X11 License
Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, ADD1LUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHOrs OR COPYRIGHT HOLDErs BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/*
 * A token-threaded Forth in C, designed to be as fast, sometimes faster, than
 * indirect threading on platforms with enough registers (eg, x86-64, ARM).
 *
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef LIB_REGEX
#include <regex.h>
#endif

#ifdef LIB_FORK
#include <sys/wait.h>
#endif

/*
#ifdef LIB_SERVE
#include <sys/socket.h>
#include <netinet/in.h>
#endif
*/

// Execution Token
typedef int16_t tok;

// Data-stack cell
// Must be able to hold a pointer!
#ifdef __LP64__
typedef int64_t cell;
#else
typedef int32_t cell;
#endif

// Forth word header
typedef struct _word {
	char *name;
	struct _word *prev, *subs;
} word;

#define MAXTOKEN 4096

enum {
	// codewords
	BYE=1, EXIT,

	IDX, LEAVE, CONT, WHILE, UNTIL, DUP, DROP, OVER, SWAP, PUSH, POP,
	TOP, NIP, ROT, TUCK, S_MY, MY, S_AT, AT, ATSP, ATFP, ATCSP, ATCFP, FETCH,
	STORE, PSTORE, CFETCH, CSTORE, CELL, NEG, ADD, SUB, SHL, SHR, MUL, DIV,
	MOD, AND, OR, XOR, LESS, MORE, EQUAL, NEQUAL, ZEQUAL, ZLESS, ZMORE, MAX,
	MIN, ADD1, SUB1, SHL1, SHR1, EXECUTE, CMOVE, NUMBER, MACRO, NORMAL,
	VALUE, VARY, CREATE, EMIT, KEY, KEYQ, ALLOCATE, RESIZE, FREE, FORMAT,
	DEPTH, HERE, ALLOT, COMMA, CCOMMA, CELLS, BYTES, ZNE, PLACE, PICK, ABS,
	SMOD, TYPE, EVALUATE, COUNT, COMPARE, GETENV, PUTENV, SLURP, BLURT, NOP,
	SYS, COLON, SCOLON, IF, ELSE, FOR, BEGIN, END, COM1, COM2, FIELDS, FIELD,
	DOES, ENTER, DOVAL, DOVAR, DOADD, DODOES, LIT_TOK, LIT_NUM, LIT_STR,
	BRANCH, JUMP, LOOP, ELOOP, MACROS, NORMALS, LATEST, ADDER, HEAD_XT,
	XT_HEAD, XT_NAME, XT_CODE, XT_BODY, XT_LIST, XT_LINK, PARSE, SPARSE, FIND,
	FINDPAIR, COMPILE, NCOMPILE, SCOMPILE, MODE, LABEL, REDOES, ONOK, ONWHAT,
	ONERROR, SOURCE, ERROR,

#ifdef LIB_REGEX
	MATCH, SPLIT,
#endif

#ifdef LIB_FORK
	FORK, SELF,
#endif

	LASTTOKEN
};

// High-level Forth code in C :-)
// It's almost readable...

tok high_evaluate[] = {
	SOURCE, FETCH, PUSH, SOURCE, STORE,
	LIT_TOK, -1, PUSH,
	LIT_TOK, -1, LOOP, 178, // 1

		DEPTH, ZLESS, BRANCH, 24, // 27
			DEPTH, ABS, LOOP, 4, LIT_TOK, 0, ELOOP,
			LIT_TOK, 0, ONERROR, FETCH, BRANCH, 7,
				DROP, LIT_TOK, 1, ONERROR, FETCH, EXECUTE,
			POP, DROP, PUSH,
			LEAVE,

		PARSE, DUP, S_MY, WHILE, // 4

		MY, CFETCH, LIT_TOK, 34, EQUAL, BRANCH, 18, // 24
			SOURCE, FETCH, MY, COUNT, SUB, SOURCE, STORE, SPARSE,
			MODE, FETCH, BRANCH, 5,
				LIT_TOK, LIT_STR, COMPILE, SCOMPILE,
			CONT,

		MY, MACROS, FIND, DUP, BRANCH, 3, // 9
			EXECUTE,
			CONT,
		DROP,

		MY, NORMALS, FIND, DUP, BRANCH, 10, // 16
			MODE, FETCH, BRANCH, 4,
				COMPILE,
			JUMP, 2,
				EXECUTE,
			CONT,
		DROP,

		MY, NUMBER, BRANCH, 10, // 14
			MODE, FETCH, BRANCH, 5,
				LIT_TOK, LIT_NUM, COMPILE, NCOMPILE,
			CONT,
		DROP,

		MY, CFETCH, LIT_TOK, 39, EQUAL, BRANCH, 37, // 43
			MY, ADD1, S_MY,

			MODE, FETCH, BRANCH, 4,
				LIT_TOK, LIT_TOK, COMPILE,

			MY, MACROS, FIND, DUP, BRANCH, 7,
				MODE, FETCH, BRANCH, 2,
					COMPILE,
				CONT,
			DROP,

			MY, NORMALS, FIND, DUP, BRANCH, 7,
				MODE, FETCH, BRANCH, 2,
					COMPILE,
				CONT,
			DROP,

		MY, NORMALS, FINDPAIR, DUP, BRANCH, 15, // 22
			MODE, FETCH, BRANCH, 5,
				COMPILE, COMPILE,
			JUMP, 6,
				SWAP, PUSH, EXECUTE, POP, EXECUTE,
			CONT,
		DROP, DROP,

		ONWHAT, FETCH, BRANCH, 8, // 11
			MY, ONWHAT, FETCH, EXECUTE, BRANCH, 2,
				CONT,

		POP, DROP, LIT_TOK, 0, PUSH, LEAVE, // 6

	ELOOP, // 1

	POP, POP, SOURCE, STORE,

	DUP, BRANCH, 10,
		ONOK, FETCH, BRANCH, 6,
			PUSH, ONOK, FETCH, EXECUTE, POP,

	EXIT
};

typedef struct {
	tok token;
	char *name;
	tok *high;
} wordinit;

wordinit list_normals[] = {
	{ .token = BYE,      .name = "bye"      },
	{ .token = EXIT,     .name = "exit"     },
	{ .token = IDX,      .name = "i"        },
	{ .token = LEAVE,    .name = "leave"    },
	{ .token = CONT,     .name = "next"     },
	{ .token = WHILE,    .name = "while"    },
	{ .token = UNTIL,    .name = "until"    },
	{ .token = DUP,      .name = "dup"      },
	{ .token = DROP,     .name = "drop"     },
	{ .token = OVER,     .name = "over"     },
	{ .token = SWAP,     .name = "swap"     },
	{ .token = NIP,      .name = "nip"      },
	{ .token = ROT,      .name = "rot"      },
	{ .token = TUCK,     .name = "tuck"     },
	{ .token = PUSH,     .name = "push"     },
	{ .token = POP,      .name = "pop"      },
	{ .token = TOP,      .name = "top"      },
	{ .token = S_MY,     .name = "my!"      },
	{ .token = MY,       .name = "my"       },
	{ .token = S_AT,     .name = "at!"      },
	{ .token = AT,       .name = "at"       },
	{ .token = ATSP,     .name = "!+"       },
	{ .token = ATFP,     .name = "@+"       },
	{ .token = ATCSP,    .name = "c!+"      },
	{ .token = ATCFP,    .name = "c@+"      },
	{ .token = FETCH,    .name = "@"        },
	{ .token = STORE,    .name = "!"        },
	{ .token = PSTORE,   .name = "+!"       },
	{ .token = CFETCH,   .name = "c@"       },
	{ .token = CSTORE,   .name = "c!"       },
	{ .token = CELL,     .name = "cell"     },
	{ .token = NEG,      .name = "neg"      },
	{ .token = ADD,      .name = "+"        },
	{ .token = SUB,      .name = "-"        },
	{ .token = SHL,      .name = "shl"      },
	{ .token = SHR,      .name = "shr"      },
	{ .token = MUL,      .name = "*"        },
	{ .token = DIV,      .name = "/"        },
	{ .token = MOD,      .name = "mod"      },
	{ .token = AND,      .name = "and"      },
	{ .token = OR,       .name = "or"       },
	{ .token = XOR,      .name = "xor"      },
	{ .token = LESS,     .name = "<"        },
	{ .token = MORE,     .name = ">"        },
	{ .token = EQUAL,    .name = "="        },
	{ .token = NEQUAL,   .name = "<>"       },
	{ .token = ZEQUAL,   .name = "0="       },
	{ .token = ZLESS,    .name = "0<"       },
	{ .token = ZMORE,    .name = "0>"       },
	{ .token = MAX,      .name = "max"      },
	{ .token = MIN,      .name = "min"      },
	{ .token = ADD1,     .name = "1+"       },
	{ .token = SUB1,     .name = "1-"       },
	{ .token = SHL1,     .name = "2*"       },
	{ .token = SHR1,     .name = "2/"       },
	{ .token = EXECUTE,  .name = "execute"  },
	{ .token = CMOVE,    .name = "cmove"    },
	{ .token = NUMBER,   .name = "number"   },
	{ .token = MACRO,    .name = "macro"    },
	{ .token = NORMAL,   .name = "normal"   },
	{ .token = VALUE,    .name = "value"    },
	{ .token = VARY,     .name = "vary"     },
	{ .token = CREATE,   .name = "create"   },
	{ .token = EMIT,     .name = "emit"     },
	{ .token = KEY,      .name = "key"      },
	{ .token = KEYQ,     .name = "key?"     },
	{ .token = ALLOCATE, .name = "allocate" },
	{ .token = RESIZE,   .name = "resize"   },
	{ .token = FREE,     .name = "free"     },
	{ .token = FORMAT,   .name = "format"   },
	{ .token = DEPTH,    .name = "depth"    },
	{ .token = HERE,     .name = "here"     },
	{ .token = ALLOT,    .name = "allot"    },
	{ .token = COUNT,    .name = "count"    },
	{ .token = COMPARE,  .name = "compare"  },
	{ .token = GETENV,   .name = "getenv"   },
	{ .token = PUTENV,   .name = "putenv"   },
	{ .token = SLURP,    .name = "slurp"    },
	{ .token = BLURT,    .name = "blurt"    },
	{ .token = NOP,      .name = "nop"      },
	{ .token = COMMA,    .name = ","        },
	{ .token = CCOMMA,   .name = "c,"       },
	{ .token = CELLS,    .name = "cells"    },
	{ .token = BYTES,    .name = "bytes"    },
	{ .token = ZNE,      .name = "0<>"      },
	{ .token = PLACE,    .name = "place"    },
	{ .token = ABS,      .name = "abs"      },
	{ .token = SMOD,     .name = "/mod"     },
	{ .token = PICK,     .name = "pick"     },
	{ .token = TYPE,     .name = "type"     },
	{ .token = ERROR,    .name = "error"    },
	{ .token = EVALUATE, .name = "evaluate", .high = high_evaluate },

#ifdef LIB_REGEX
	{ .token = MATCH,    .name = "match"    },
	{ .token = SPLIT,    .name = "split"    },
#endif

#ifdef LIB_FORK
	{ .token = FORK,     .name = "fork"     },
	{ .token = SELF,     .name = "self"     },
#endif
};

wordinit list_macros[] = {
	{ .token = COLON,    .name = ":"        },
	{ .token = SCOLON,   .name = ";"        },
	{ .token = IF,       .name = "if"       },
	{ .token = ELSE,     .name = "else"     },
	{ .token = FOR,      .name = "for"      },
	{ .token = BEGIN,    .name = "begin"    },
	{ .token = END,      .name = "end"      },
	{ .token = COM1,     .name = "\\"       },
	{ .token = COM2,     .name = "("        },
	{ .token = FIELDS,   .name = "record"   },
	{ .token = FIELD,    .name = "field"    },
	{ .token = DOES,     .name = "does"     },
};

wordinit list_hiddens[] = {
	{ .token = ENTER,    .name = "enter"    },
	{ .token = DOVAL,    .name = "doval"    },
	{ .token = DOVAR,    .name = "dovar"    },
	{ .token = DOADD,    .name = "doadd"    },
	{ .token = DODOES,   .name = "dodoes"   },
	{ .token = LIT_TOK,  .name = "lit_tok"  },
	{ .token = LIT_NUM,  .name = "lit_num"  },
	{ .token = LIT_STR,  .name = "lit_str"  },
	{ .token = BRANCH,   .name = "branch"   },
	{ .token = JUMP,     .name = "jump"     },
	{ .token = LOOP,     .name = "loop"     },
	{ .token = ELOOP,    .name = "eloop"    },
	{ .token = MACROS,   .name = "macros"   },
	{ .token = NORMALS,  .name = "normals"  },
	{ .token = LATEST,   .name = "latest"   },
	{ .token = ADDER,    .name = "adder"    },
	{ .token = HEAD_XT,  .name = "head-xt"  },
	{ .token = XT_HEAD,  .name = "xt-head"  },
	{ .token = XT_NAME,  .name = "xt-name"  },
	{ .token = XT_CODE,  .name = "xt-code"  },
	{ .token = XT_BODY,  .name = "xt-body"  },
	{ .token = XT_LIST,  .name = "xt-list"  },
	{ .token = XT_LINK,  .name = "xt-link"  },
	{ .token = PARSE,    .name = "parse"    },
	{ .token = SPARSE,   .name = "sparse"   },
	{ .token = FIND,     .name = "find"     },
	{ .token = FINDPAIR, .name = "findpair" },
	{ .token = COMPILE,  .name = "compile"  },
	{ .token = NCOMPILE, .name = "ncompile" },
	{ .token = SCOMPILE, .name = "scompile" },
	{ .token = MODE,     .name = "mode"     },
	{ .token = LABEL,    .name = "label"    },
	{ .token = REDOES,   .name = "redoes"   },
	{ .token = ONOK,     .name = "on-ok"    },
	{ .token = ONWHAT,   .name = "on-what"  },
	{ .token = ONERROR,  .name = "on-error" },
	{ .token = SOURCE,   .name = "source"   },
};

// Some Forth global variables
cell source, mode, on_ok, on_error, on_what;
word *macro, *normal, **current;

// Code space
tok code[1024*1024];

// Head space
word head[MAXTOKEN];

// token -> cfa map
void *call[MAXTOKEN];

// token -> bfa map
tok *body[MAXTOKEN];

// Data stack
#define dpush(t) (*dsp++ = (t))
#define dpop (*--dsp)

// Return stack
#define rpush(t) (*rsp++ = (t))
#define rpop (*--rsp)

// Read a file into memory
char*
slurp(char *name)
{
	int len = 0, lim = 1024;
	char *pad = malloc(lim+1);

	FILE *file = fopen(name, "r");
	if (!file)
	{
		fprintf(stderr, "could not open: %s\n", name);
		return NULL;
	}

	unsigned int read = 0;
	for (;;)
	{
		read = fread(pad+len, 1, 1024, file);
		len += read; lim += 1024; pad[len] = 0;
		if (read < 1024) break;
		pad = realloc(pad, lim+1);
	}
	fclose(file);
	return pad;
}

// Write a file to disk
int
blurt(const char *name, char *data, unsigned int dlen)
{
	FILE *f = fopen(name, "w+");
	if (!f || fwrite(data, 1, dlen, f) != dlen) return 0;
	fclose(f);
	return 1;
}

// Compile an execution token to code space
void
compile(tok n, tok **p)
{
	tok *cp = *p;
	*cp++ = n;
	*p = cp;
}

// Compile a number to code space
void
ncompile(cell n, tok **p)
{
	tok *cp = *p;
	*((cell*)cp) = n;
	cp = (tok*)(((char*)cp) + sizeof(cell));
	*p = cp;
}

// Compile a counted string to code space
void
scompile(char *s, tok **p)
{
	tok *cp = *p;
	tok *tokp = cp++;
	char *d = (char*)cp;
	while (*s) *d++ = *s++; *d++ = 0;
	while ((d - (char*)cp) % sizeof(tok)) *d++ = 0;
	cp = (tok*)d;
	*tokp = cp - tokp;
	*p = cp;
}

// Start a branch (IF, ELSE)
tok*
mark(tok **p)
{
	tok *cp = *p;
	compile(0, p);
	return cp;
}

// Resolve a branch (THEN)
void
patch(tok *tokp, tok **p)
{
	tok *cp = *p;
	*tokp = cp - tokp;
}

char parsed[1024];

// Parse a white-space delimited word from source
char*
parse()
{
	int len = 0;
	char *s = (char*)source;
	while (*s && *s < 33) s++;
	while (*s && *s > 32 && len < 1023) parsed[len++] = *s++;
	parsed[len] = 0;
	source = (cell)s;
	return len ? parsed: NULL;
}

int sparse_i;
#define SPARSE_BUF 1024*1
#define SPARSE_BUFS 3
char *sparse_bufs[SPARSE_BUFS];

// Parse a quote delimited string literal from source
char*
sparse()
{
	char *buf = malloc(SPARSE_BUF);

	int idx = sparse_i++;
	if (sparse_i == SPARSE_BUFS) sparse_i = 0;

	free(sparse_bufs[idx]);
	sparse_bufs[idx] = buf;

	int len = 0; char c, *s = (char*)source+1, e = '"';
	while ((c = *s++) && len < 1023)
	{
		if (c == e)
			break;
		if (c == '\\' && *s)
		{
			c = *s++;
			if (c == 'n')
				c = '\n';
			else
			if (c == 'r')
				c = '\r';
			else
			if (c == 'e')
				c = '\e';
			else
			if (c == 't')
				c = '\t';
			else
			if (c == e)
				c = e;
			else
			{
				c = '\\';
				s--;
			}
		}
		buf[len++] = c;
	}
	buf[len] = 0;
	source = (cell)s;
	return buf;
}

// Parse a name and create a new word header
tok
label(word **p)
{
	word *hp = *p;
	parse();
	word *w = hp++;
	w->name = strdup(parsed);
	w->prev = *current;
	w->subs = w;
	*current = w;
	*p = hp;
	return w-head;
}

int format_i;
#define FORMAT_BUF 1024*64
#define FORMAT_BUFS 3
char *format_bufs[FORMAT_BUFS];

// Format a string using C-like printf() syntax
char*
format(char *pat, cell **_dsp)
{
	cell *dsp = *_dsp;

	char *buf = malloc(FORMAT_BUF);

	int idx = format_i++;
	if (format_i == FORMAT_BUFS) format_i = 0;

	free(format_bufs[idx]);
	format_bufs[idx] = buf;

	char tmp[32], *p;
	char *in = pat, *out = buf;

	while (*in)
	{
		char c = *in++;
		if (*in && c == '%')
		{
			if (*in == '%')
				in++;
			else
			{
				p = tmp;
				*p++ = c;
				do { c = *in++; *p++ = c; }
				while (!strchr("cdieEfgGosuxX", c));
				*p = 0;

				if (c == 's')
				{
					out += sprintf(out, tmp, (char*)dpop);
				}
				else
				if (strchr("cdiouxX", c))
				{
					out += sprintf(out, tmp, dpop);
				}
				else
				if (strchr("eEfgG", c))
				{
					out += sprintf(out, tmp, (double)dpop);
				}
				continue;
			}
		}
		*out++ = c;
	}
	*out = 0;
	*_dsp = dsp;
	return buf;
}

//#define same_name(a,b) ((a)[0] == (b)[0] && !strcmp((a), (b)))
#define same_name(a,b) (((int16_t*)(a))[0] == ((int16_t*)(b))[0] && !strcmp((a), (b)))

// Find a name in a wordlist
tok
find(word *w, char *name)
{
	if (!name[0])
	{
		return 0;
	}
	else
	{
		char *split = NULL;
		if ((split = strchr(name, ':')) && split[1] && split > name)
		{
			*split = 0;
			char *outer = name, *inner = split+1;

			while (w && !same_name(w->name, outer))
				w = w->prev;

			if (w)
			{
				word *s = w->subs;
				while (s && s != w && !same_name(s->name, inner))
					s = s->prev;
				w = s != w ? s: NULL;
			}

			*split = ':';
		}
		else
		{
			while (w && !same_name(w->name, name))
				w = w->prev;
		}
		return w ? (w - head): 0;
	}
}

// Find a namespace:name pair in a wordlist
tok
findpair(word *w, char *name, tok *xt)
{
	cell a = 0, b = 0;

	char *split = NULL;
	if (name[0] && name[0] != '.' && (split = strchr(name, '.')))
	{
		*split = 0;
		char *outer = name, *inner = split+1;

		while (w && !same_name(w->name, outer))
			w = w->prev;

		if (w)
		{
			word *s = w->subs;
			while (s && !same_name(s->name, inner))
				s = s->prev;

			if (s)
			{
				a = w-head;
				b = s-head;
			}
		}
		*split = '.';
	}

	*xt = b;
	return a;
}

// Convert a string to a number
cell
number(char *conv, cell *res)
{
	char str[32];

	*res = 0;
	if (!conv)
		return 0;

	// number
	int len = strlen(conv), base = 10;

	if (len > 31) len = 31;
	strncpy(str, conv, len);
	str[len] = 0;

	char *err, suffix = str[len-1];
	// hex
	if (suffix == 'h')
	{
		base = 16;
		str[len-1] = 0;
	}
	else
	// bin
	if (suffix == 'b')
	{
		base = 2;
		str[len-1] = 0;
	}
	else
	// hex again
	if (!strncmp(str, "0x", 2))
	{
		base = 16;
	}
	if (str[0])
	{
		*res = strtol(str, &err, base);
		return (err == str + strlen(str)) ? -1: 0;
	}
	return 0;
}

#ifdef LIB_REGEX

// POSIX regex match
int
match(char *pattern, char **subject)
{
	regex_t re; int r = 0; regmatch_t pmatch;
	if (regcomp(&re, pattern, REG_EXTENDED) != 0)
	{
		fprintf(stderr, "regex compile failure: %s", pattern);
		exit(1);
	}
	if (subject)
	{
		r = regexec(&re, *subject, 1, &pmatch, 0) == 0 ?-1:0;
		*subject += r ? pmatch.rm_so: strlen(*subject);
	}
	regfree(&re);
	return r;
}

// POSIX regex split
int
split(char *pattern, char **subject)
{
	regex_t re; int r = 0; regmatch_t pmatch;
	if (regcomp(&re, pattern, REG_EXTENDED) != 0)
	{
		fprintf(stderr, "regex compile failure: %s", pattern);
		exit(1);
	}
	if (*subject)
	{
		r = regexec(&re, *subject, 1, &pmatch, 0) == 0 ?-1:0;
		if (r)
		{
			memset(*subject + pmatch.rm_so, 0, pmatch.rm_eo - pmatch.rm_so);
			*subject += pmatch.rm_eo;
		}
		else
		{
			*subject += strlen(*subject);
		}
	}
	regfree(&re);
	return r;
}

#endif

#ifdef LIB_FORK

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

#endif

tok init[] = { EVALUATE, BYE };

#include "src_base.c"

// Use GCC's &&label syntax to find code word adresses.
#define CODE(x) call[(x)] = &&code_##x; if (0) { code_##x:

// Inline NEXT
#define INEXT xt = *ip++; goto *call[xt];

#ifdef DEBUG
#define NEXT goto next; }
#else
#define NEXT INEXT }
#endif

int
main(int argc, char *argv[], char *env[])
{
	cell ds[32]; // Data stack
	cell rs[32]; // Return stack
	cell as[32]; // Alternate data stack (PUSH, POP, TOP)

	// Both these variables are used in NEXT and should have first-dibs on being in
	// a register. The C compiler is free to ignore the "register" keyword, but some
	// benchmarking indicates it's still a good idea in some situations:
	// - Generally improves performance when optimization level is less than -O3
	// - Always improves performance when the target platform is register-starved
	register tok* ip; // Instruction pointer
	register tok xt;  // Last token loaded by NEXT (xt of currently executing code word)

	cell *dsp; // Data stack pointer
	cell *rsp; // Return stack pointer
	cell *asp; // Alternate data stack pointer

	int i;
	tok *tokp, exec[2], *cp;
	cell *cellp, tmp, tos, num;
	char *charp, *s1, *s2;
	cell index, limit, src, dst;
	word *w, *hp;
	void *voidp;

	// Assume the last command line argument is our Forth source file
	char *run = NULL;
	for (i = 1; i < argc; i++)
		run = argv[i];

	for (i = 0; env[i]; i++)
		fprintf(stderr, "%s\n", env[i]);

	// Initialize the dictionary headers

	word *last = NULL;

	for (i = 0; i < sizeof(list_normals)/sizeof(wordinit); i++)
	{
		hp = &head[list_normals[i].token];

		hp->name = list_normals[i].name;
		if (list_normals[i].high)
		{
			call[hp-head] = &&code_ENTER;
			body[hp-head] = list_normals[i].high;
		}

		hp->subs = hp;
		hp->prev = last;
		last = hp;
	}

	hp = &head[SYS];
	word *sys = hp;
	hp->name = "sys";
	call[hp-head] = &&code_NOP;
	hp->subs = hp;
	hp->prev = last;
	last = hp;
	hp++;

	for (i = 0; i < sizeof(list_hiddens)/sizeof(wordinit); i++)
	{
		hp = &head[list_hiddens[i].token];

		hp->name = list_hiddens[i].name;
		if (list_hiddens[i].high)
		{
			call[hp-head] = &&code_ENTER;
			body[hp-head] = list_hiddens[i].high;
		}

		hp->subs = hp;
		hp->prev = last;
		last = hp;
	}

	sys->subs = last;
	last = sys;

	normal = last;
	last = NULL;

	for (i = 0; i < sizeof(list_macros)/sizeof(wordinit); i++)
	{
		hp = &head[list_macros[i].token];

		hp->name = list_macros[i].name;
		if (list_macros[i].high)
		{
			call[hp-head] = &&code_ENTER;
			body[hp-head] = list_macros[i].high;
		}

		hp->subs = hp;
		hp->prev = last;
		last = hp;
	}

	macro = last;
	current = &normal;

	hp = &head[LASTTOKEN];
	cp = code;

	// Initialize virtual machine

	memset(ds, 0, sizeof(ds));
	dsp = ds+3;
	rsp = rs+3;
	asp = as+3;
	ip = init;

	// Look for a Forth source file on the command line

	char *fsrc = strdup(pfft_src_base);

	if (run)
	{
		char *pad = slurp(run);
		if (pad)
		{
			fsrc = realloc(fsrc, strlen(fsrc) + 2 + strlen(pad));
			strcat(fsrc, "\n");
			strcat(fsrc, pad);
		}
	}
	else
	{
		fsrc = realloc(fsrc, strlen(fsrc) + 50);
		strcat(fsrc, "\nshell");
	}

	// First word called is always EVALUATE, so place source address TOS
	tos = (cell)fsrc;

	// Start code word labels

	// These *must* come before the first INEXT executes because CODE
	// generates code to initialize the call[] array at runtime...

	// ( -- )
	CODE(BYE)
		exit(0);
	NEXT

	// ( -- )
	CODE(ENTER)
		*rsp = (cell)ip;
		rsp += 3;
		ip = body[xt];
	NEXT

	// ( -- )
	CODE(EXIT)
		rsp -= 3;
		ip = (tok*)*rsp;
	NEXT

	// ( xt -- )
	CODE(EXECUTE)
		xt = tos;
		tos = dpop;
		goto *call[xt];
	NEXT

	// ( --  a )
	CODE(DOVAR)
		dpush(tos);
		tos = (cell)(body[xt]);
	NEXT

	// ( -- n )
	CODE(DOVAL)
		dpush(tos);
		tos = *((cell*)(body[xt]));
	NEXT

	// ( a -- a+n )
	CODE(DOADD)
		tos = tos + *((cell*)(body[xt]));
	NEXT

	// ( -- a )
	CODE(DODOES)
		*rsp = (cell)ip;
		rsp += 3;
		dpush(tos);
		tos = ((cell*)(body[xt]))[0];
		ip = (tok*)(((cell*)(body[xt]))[1]);
	NEXT

	// ( n -- n n )
	CODE(DUP)
		dpush(tos);
	NEXT

	// ( n -- )
	CODE(DROP)
		tos = dpop;
	NEXT

	// ( n m -- n m n )
	CODE(OVER)
		dpush(tos);
		tos = dsp[-2];
	NEXT

	// ( n m -- m n )
	CODE(SWAP)
		tmp = dsp[-1];
		dsp[-1] = tos;
		tos = tmp;
	NEXT

	// ( n m -- m )
	CODE(NIP)
		dsp--;
	NEXT

	// ( n m o -- m o n )
	CODE(ROT)
		tmp = dsp[-2];
		dsp[-2] = dsp[-1];
		dsp[-1] = tos;
		tos = tmp;
	NEXT

	// ( n m -- m n m )
	CODE(TUCK)
		tmp = dsp[-1];
		*dsp++ = tmp;
		dsp[-2] = tos;
	NEXT

	// ( n -- )
	CODE(PUSH)
		*asp++ = tos;
		tos = dpop;
	NEXT

	// ( -- n )
	CODE(POP)
		dpush(tos);
		tos = *--asp;
	NEXT

	// ( -- n )
	CODE(TOP)
		dpush(tos);
		tos = asp[-1];
	NEXT

	// ( n -- )
	CODE(S_MY)
		rsp[-1] = tos;
		tos = dpop;
	NEXT

	// ( -- n )
	CODE(MY)
		dpush(tos);
		tos = rsp[-1];
	NEXT

	// ( a -- )
	CODE(S_AT)
		rsp[-2] = tos;
		tos = dpop;
	NEXT

	// ( -- a )
	CODE(AT)
		dpush(tos);
		tos = rsp[-2];
	NEXT

	// ( n -- )
	CODE(ATSP)
		cellp = (cell*)(rsp[-2]);
		*cellp++ = tos;
		rsp[-2] = (cell)cellp;
		tos = dpop;
	NEXT

	// ( -- n )
	CODE(ATFP)
		dpush(tos);
		cellp = (cell*)(rsp[-2]);
		tos = *cellp++;
		rsp[-2] = (cell)cellp;
	NEXT

	// ( c -- )
	CODE(ATCSP)
		charp = (char*)(rsp[-2]);
		*charp++ = tos;
		rsp[-2] = (cell)charp;
		tos = dpop;
	NEXT

	// ( -- c )
	CODE(ATCFP)
		dpush(tos);
		charp = (char*)(rsp[-2]);
		tos = *charp++;
		rsp[-2] = (cell)charp;
	NEXT

	// ( -- xt )
	CODE(LIT_TOK)
		dpush(tos);
		tos = *ip++;
	NEXT

	// ( -- n )
	CODE(LIT_NUM)
		dpush(tos);
		tos = *((cell*)ip);
		ip = (tok*)(((char*)ip) + sizeof(cell));
	NEXT

	// ( -- a )
	CODE(LIT_STR)
		dpush(tos);
		tos = (cell)(ip+1);
		ip += *ip;
	NEXT

	// ( f -- )
	CODE(BRANCH)
		ip += tos ? 1: *ip;
		tos = dpop;
	NEXT

	// ( -- )
	CODE(JUMP)
		ip += *ip;
	NEXT

	// ( n -- )
	CODE(LOOP)
		if (tos)
		{
			rsp[0] = rsp[-3];
			rsp[1] = rsp[-2];
			rsp[2] = rsp[-1];
			rsp[-3] = (cell)ip; // break
			rsp[-2] = tos; // limit
			rsp[-1] = 0; // index
			rsp += 3;
			ip++;
		}
		else
		{
			ip += *ip;
		}
		tos = dpop;
	NEXT

	// ( -- )
	CODE(ELOOP)
		index = rsp[-4];
		limit = rsp[-5];
		if (++index < limit || limit < 0)
		{
			ip = (tok*)(rsp[-6]);
			rsp[-4] = index;
			ip++;
		}
		else
		{
			rsp -= 3;
			rsp[-3] = rsp[0];
			rsp[-2] = rsp[1];
			rsp[-1] = rsp[2];
		}
	NEXT

	// ( -- i )
	CODE(IDX)
		dpush(tos);
		tos = rsp[-4];
	NEXT

	// ( -- )
	CODE(LEAVE)
		ip = (tok*)(rsp[-6]);
		rsp -= 3;
		rsp[-3] = rsp[0];
		rsp[-2] = rsp[1];
		rsp[-1] = rsp[2];
		ip += *ip;
	NEXT

	// ( -- )
	CODE(CONT)
		ip = (tok*)(rsp[-6]);
		ip += (*ip)-1;
	NEXT

	// ( f -- )
	CODE(WHILE)
		tmp = tos;
		tos = dpop;
		if (!tmp)
			goto code_LEAVE;
	NEXT

	// ( f -- )
	CODE(UNTIL)
		tmp = tos;
		tos = dpop;
		if (tmp)
			goto code_LEAVE;
	NEXT

	// ( a -- n )
	CODE(FETCH)
		tos = *((cell*)tos);
	NEXT

	// ( n a -- )
	CODE(STORE)
		*((cell*)tos) = dpop;
		tos = dpop;
	NEXT

	// ( n a -- )
	CODE(PSTORE)
		*((cell*)tos) += dpop;
		tos = dpop;
	NEXT

	// ( -- c )
	CODE(CFETCH)
		tos = *((char*)tos);
	NEXT

	// ( c a -- )
	CODE(CSTORE)
		*((char*)tos) = dpop;
		tos = dpop;
	NEXT

	// ( -- n )
	CODE(CELL)
		dpush(tos);
		tos = sizeof(cell);
	NEXT

	// ( n -- -n )
	CODE(NEG)
		tos *= -1;
	NEXT

	// ( n m -- n+m )
	CODE(ADD)
		tos = dpop + tos;
	NEXT

	// ( n m -- n-m )
	CODE(SUB)
		tos = dpop - tos;
	NEXT

	// ( n m -- n<<m )
	CODE(SHL)
		tos = dpop << tos;
	NEXT

	// ( n m -- n>>m)
	CODE(SHR)
		tos = dpop >> tos;
	NEXT

	// ( n m -- n*m )
	CODE(MUL)
		tos = dpop * tos;
	NEXT

	// ( n m -- n/m )
	CODE(DIV)
		tos = dpop / tos;
	NEXT

	// ( n m -- n%m )
	CODE(MOD)
		tos = dpop % tos;
	NEXT

	// ( n m -- n&m )
	CODE(AND)
		tos = dpop & tos;
	NEXT

	// ( n m -- n|m )
	CODE(OR)
		tos = dpop | tos;
	NEXT

	// ( n m -- n^m )
	CODE(XOR)
		tos = dpop ^ tos;
	NEXT

	// ( n m -- f )
	CODE(LESS)
		tos = dpop < tos ? -1:0;
	NEXT

	// ( n m -- f )
	CODE(MORE)
		tos = dpop > tos ? -1:0;
	NEXT

	// ( n m -- f )
	CODE(EQUAL)
		tos = dpop == tos ? -1:0;
	NEXT

	// ( n m -- f )
	CODE(NEQUAL)
		tos = dpop != tos ? -1:0;
	NEXT

	// ( n -- f )
	CODE(ZEQUAL)
		tos = tos == 0 ? -1:0;
	NEXT

	// ( n -- f )
	CODE(ZLESS)
		tos = tos < 0 ? -1:0;
	NEXT

	// ( n -- f )
	CODE(ZMORE)
		tos = tos > 0 ? -1:0;
	NEXT

	// ( n -- f )
	CODE(ZNE)
		tos = tos != 0 ? -1:0;
	NEXT

	// ( n m -- o )
	CODE(MIN)
		tmp = dpop;
		tos = tos < tmp ? tos: tmp;
	NEXT

	// ( n m -- o )
	CODE(MAX)
		tmp = dpop;
		tos = tos > tmp ? tos: tmp;
	NEXT

	// ( n -- n+1 )
	CODE(ADD1)
		tos = tos+1;
	NEXT

	// ( n -- n-1 )
	CODE(SUB1)
		tos = tos-1;
	NEXT

	// ( n -- n<<1 )
	CODE(SHL1)
		tos = tos << 1;
	NEXT

	// ( n -- n>>1 )
	CODE(SHR1)
		tos = tos >> 1;
	NEXT

	// ( src dst len -- )
	CODE(CMOVE)
		dst = dpop, src = dpop;
		memmove((char*)dst, (char*)src, tos);
		tos = dpop;
	NEXT

	// ( -- a )
	CODE(PARSE)
		dpush(tos);
		tos = (cell)parse();
	NEXT

	// ( -- a )
	CODE(SPARSE)
		dpush(tos);
		tos = (cell)sparse();
	NEXT

	// ( xt -- )
	CODE(COMPILE)
		compile(tos, &cp);
		tos = dpop;
	NEXT

	// ( n -- )
	CODE(NCOMPILE)
		ncompile(tos, &cp);
		tos = dpop;
	NEXT

	// ( a -- )
	CODE(SCOMPILE)
		scompile((char*)tos, &cp);
		tos = dpop;
	NEXT

	// ( -- a )
	CODE(SOURCE)
		dpush(tos);
		tos = (cell)&source;
	NEXT

	// ( -- a )
	CODE(MODE)
		dpush(tos);
		tos = (cell)&mode;
	NEXT

	// ( -- )
	CODE(MACROS)
		dpush(tos);
		tos = (cell)macro;
	NEXT

	// ( -- )
	CODE(NORMALS)
		dpush(tos);
		tos = (cell)normal;
	NEXT

	// ( -- a )
	CODE(LATEST)
		dpush(tos);
		tos = (cell)current;
	NEXT

	// ( -- )
	CODE(MACRO)
		current = &macro;
	NEXT

	// ( -- )
	CODE(NORMAL)
		current = &normal;
	NEXT

	// ( -- xt )
	CODE(LABEL)
		dpush(tos);
		if (hp-head == MAXTOKEN)
		{
			fprintf(stderr, "out of tokens!\n");
			exit(1);
		}
		xt = label(&hp);
		call[xt] = &&code_ENTER;
		body[xt] = cp;
		tos = xt;
	NEXT

	// ( -- )
	CODE(CREATE)
		xt = label(&hp);
		call[xt] = &&code_DOVAR;
		body[xt] = cp;
	NEXT

	// ( n -- )
	CODE(VALUE)
		xt = label(&hp);
		call[xt] = &&code_DOVAL;
		body[xt] = cp;
		goto code_NCOMPILE;
	NEXT

	// ( n -- )
	CODE(ADDER)
		xt = label(&hp);
		call[xt] = &&code_DOADD;
		body[xt] = cp;
		goto code_NCOMPILE;
	NEXT

	// ( xt -- )
	CODE(REDOES)
		w = *current;
		w->subs = &head[tos];
		xt = w-head;
		call[xt] = &&code_DODOES;
		tokp = cp;
		ncompile((cell)body[xt], &cp);
		ncompile((cell)ip, &cp);
		body[xt] = tokp;
		tos = dpop;
		goto code_EXIT;
	NEXT

	// ( n xt -- )
	CODE(VARY)
		*((cell*)(body[tos])) = dpop;
		tos = dpop;
	NEXT

	// ( a -- xt )
	CODE(HEAD_XT)
		tos = ((word*)tos)-head;
	NEXT

	// ( xt -- a )
	CODE(XT_HEAD)
		tos = (cell)(&head[tos]);
	NEXT

	// ( xt -- a )
	CODE(XT_NAME)
		tos = (cell)(&head[tos].name);
	NEXT

	// ( xt -- a )
	CODE(XT_CODE)
		tos = (cell)(&call[tos]);
	NEXT

	// ( xt -- a )
	CODE(XT_BODY)
		tos = (cell)(&body[tos]);
	NEXT

	// ( xt -- a )
	CODE(XT_LIST)
		tos = (cell)(&head[tos].subs);
	NEXT

	// ( xt -- a )
	CODE(XT_LINK)
		tos = (cell)(&head[tos].prev);
	NEXT

	// ( a -- n )
	CODE(COUNT)
		tos = tos ? strlen((char*)tos): 0;
	NEXT

	// ( s1 s2 -- n )
	CODE(COMPARE)
		s1 = (char*)dpop;
		s2 = (char*)tos;
		if (!s1 && !s2)
			tos = 0;
		else
		if (s1 && !s2)
			tos = 1;
		else
		if (!s1 && s2)
			tos = -1;
		else
			tos = strcmp(s1, s2);
	NEXT

	// ( c -- )
	CODE(EMIT)
		fputc(tos, stdout);
		tos = dpop;
	NEXT

	// ( -- c )
	CODE(KEY)
		dpush(tos);
		tos = fgetc(stdin);
	NEXT

	// ( -- f )
	CODE(KEYQ)
		dpush(tos);
		tos = feof(stdin) ? 0:-1;
	NEXT

	// ( -- a )
	CODE(GETENV)
		tos = (cell)getenv((char*)tos);
	NEXT

	// ( a -- )
	CODE(PUTENV)
		putenv((char*)tos);
		tos = dpop;
	NEXT

	// ( -- a )
	CODE(ONOK)
		dpush(tos);
		tos = (cell)&on_ok;
	NEXT

	// ( -- a )
	CODE(ONWHAT)
		dpush(tos);
		tos = (cell)&on_what;
	NEXT

	// ( -- a )
	CODE(ONERROR)
		dpush(tos);
		tos = (cell)&on_error;
	NEXT

	// ( n -- a )
	CODE(ALLOCATE)
		if (tos < sizeof(cell))
			tos = sizeof(cell);
		tos = (cell)calloc(1, tos);
	NEXT

	// ( a n -- b )
	CODE(RESIZE)
		voidp = (void*)dpop;
		tos = (cell)realloc(voidp, tos);
	NEXT

	// ( a -- )
	CODE(FREE)
		free((void*)tos);
		tos = dpop;
	NEXT

	// ( name -- a )
	CODE(SLURP)
		tos = (cell)slurp((char*)tos);
	NEXT

	// ( a name -- f )
	CODE(BLURT)
		charp = (char*)dpop;
		tos = (cell)blurt((char*)tos, charp, strlen(charp));
	NEXT

	// ( ... a -- b )
	CODE(FORMAT)
		cellp = dsp;
		tos = (cell)format((char*)tos, &cellp);
		dsp = cellp;
	NEXT

	// ( name a -- xt )
	CODE(FIND)
		tos = find((word*)tos, (char*)dpop);
	NEXT

	// ( name a -- xt2 xt )
	CODE(FINDPAIR)
		tos = findpair((word*)tos, (char*)dpop, exec);
		dpush(exec[0]);
	NEXT

	// ( a -- n f )
	CODE(NUMBER)
		tos = number((char*)tos, &num);
		dpush(num);
	NEXT

	// ( -- n )
	CODE(DEPTH)
		dpush(tos);
		tos = dsp-ds-3;
	NEXT

	// ( -- a )
	CODE(HERE)
		dpush(tos);
		tos = (cell)cp;
	NEXT

	// ( n -- )
	CODE(ALLOT)
		memset(cp, 0, tos);
		cp = (tok*)((char*)cp + tos);
		tos = dpop;
	NEXT

	// ( n -- )
	CODE(COMMA)
		ncompile(tos, &cp);
		tos = dpop;
	NEXT

	// ( c -- )
	CODE(CCOMMA)
		*((char*)cp) = tos;
		cp = (tok*)((char*)cp + sizeof(char));
		tos = dpop;
	NEXT

	// ( n -- m )
	CODE(CELLS)
		tos *= sizeof(cell);
	NEXT

	// ( n -- m )
	CODE(BYTES)

	NEXT

	// ( src dst -- )
	CODE(PLACE)
		tmp = dpop;
		*((char*)tos) = 0;
		if (tmp)
			strcpy((char*)tos, (char*)tmp);
		tos = dpop;
	NEXT

	// ( n -- m )
	CODE(ABS)
		if (tos < 0)
			tos *= -1;
	NEXT

	// ( n m -- r q )
	CODE(SMOD)
		tmp = dpop;
		dpush(tmp % tos);
		tos = tmp / tos;
	NEXT

	// ( -- a xt )
	CODE(COLON)
		dpush(tos);
		mode++;
		compile(JUMP, &cp);
		dpush((cell)mark(&cp));
		xt = label(&hp);
		call[xt] = &&code_ENTER;
		body[xt] = cp;
		tos = xt;
	NEXT

	// ( a xt -- )
	CODE(SCOLON)
		compile(EXIT, &cp);
		mode--;
		head[tos].subs = *current;
		*current = &head[tos];
		patch((tok*)dpop, &cp);
		tos = dpop;
	NEXT

	// ( -- a n )
	CODE(IF)
		dpush(tos);
		if (!mode)
			dpush((cell)cp);
		mode++;
		compile(BRANCH, &cp);
		dpush((cell)mark(&cp));
		tos = 1;
	NEXT

	// ( a n -- b n )
	CODE(ELSE)
		tos = dpop;
		compile(JUMP, &cp);
		dpush((cell)mark(&cp));
		patch((tok*)tos, &cp);
		tos = 1;
	NEXT

	// ( -- a n )
	CODE(FOR)
		dpush(tos);
		if (!mode)
			dpush((cell)cp);
		mode++;
		compile(LOOP, &cp);
		dpush((cell)mark(&cp));
		tos = 2;
	NEXT

	// ( -- a n )
	CODE(BEGIN)
		dpush(tos);
		if (mode)
		{
			compile(LIT_TOK, &cp);
			compile(-1, &cp);
		}
		else
		{
			tos = -1;
			dpush((cell)cp);
		}
		mode++;
		compile(LOOP, &cp);
		dpush((cell)mark(&cp));
		tos = 2;
	NEXT

	// ( ... a n -- )
	CODE(END)
		if (tos == 1)
		{
			patch((tok*)dpop, &cp);
		}
		else
		if (tos == 2)
		{
			compile(ELOOP, &cp);
			patch((tok*)dpop, &cp);
		}
		else
		if (tos == 3)
		{
			tmp = dpop; // size
			tos = dpop; // xt
			patch((tok*)dpop, &cp);
			mode = dpop;
			dpush(tmp);
			goto code_VARY;
		}
		if (tos == 1 || tos == 2)
		{
			mode--;
			if (!mode)
			{
				compile(EXIT, &cp);
				*rsp = (cell)ip;
				rsp += 3;
				ip = (tok*)dpop;
			}
		}
		tos = dpop;
	NEXT

	// ( -- f a xt 0 n )
	CODE(FIELDS)
		dpush(tos);

		dpush(mode);
		mode = 0;

		compile(JUMP, &cp);
		dpush((cell)mark(&cp));

		xt = label(&hp);
		call[xt] = &&code_DOVAL;
		body[xt] = cp;
		ncompile(0, &cp);
		dpush(xt);

		dpush(0);
		tos = 3;
	NEXT

	// ( size n m -- size+m n )
	CODE(FIELD)

		tmp = dpop;
		tmp = dpop;

		xt = label(&hp);
		call[xt] = &&code_DOADD;
		body[xt] = cp;

		ncompile(tmp, &cp);
		tmp += tos;

		dpush(tmp);
		tos = 3;
	NEXT

	// ( -- )
	CODE(COM1)
		charp = (char*)source;
		while (*charp && *charp++ != '\n');
		source = (cell)charp;
	NEXT

	// ( -- )
	CODE(COM2)
		charp = (char*)source;
		while (*charp && *charp++ != ')');
		source = (cell)charp;
	NEXT

	// ( -- )
	CODE(DOES)
		compile(LIT_TOK, &cp);
		compile(*current-head, &cp);
		compile(REDOES, &cp);
	NEXT

	// ( ... n -- m )
	CODE(PICK)
		tos = dsp[(tos+1)*-1];
	NEXT

	// ( a -- )
	CODE(TYPE)
		printf("%s", (char*)tos);
		tos = dpop;
	NEXT

	// ( a -- )
	CODE(ERROR)
		fprintf(stderr, "%s", (char*)tos);
		tos = dpop;
	NEXT

#ifdef LIB_REGEX

	// ( subject pattern -- subject' f )
	CODE(MATCH)
		charp = (char*)dpop;
		tos = match((char*)tos, &charp);
		dpush((cell)charp);
	NEXT

	// ( subject pattern -- subject' f )
	CODE(SPLIT)
		charp = (char*)dpop;
		tos = split((char*)tos, &charp);
		dpush((cell)charp);
	NEXT

#endif

#ifdef LIB_FORK

	// ( -- )
	CODE(FORK)
		signal(SIGCHLD, catch_exit);
		if (!fork())
		{
			setsid();
			init[0] = BYE;
			ip = init;
			xt = tos;
			tos = dpop;
			dsp = ds+3;
			rsp = rs+3;
			asp = as+3;
			goto *call[xt];
		}
		tos = dpop;
	NEXT

	// ( -- pid )
	CODE(SELF)
		dpush(tos);
		tos = getpid();
	NEXT

#endif

	// ( -- )
	CODE(NOP)

	NEXT

#ifdef DEBUG
	for (i = 1; i < LASTTOKEN; i++)
	{
		if (!call[i])
			fprintf(stderr, "TODO: %d %s\n", i, head[i].name);
	}

	next:
#endif

	// Finally, jump into Forth!
	INEXT

	// Should never get here...
	return 0;
}
