/*

MIT/X11 License
Copyright (c) 2012-2015 Sean Pringle <sean.pringle@gmail.com>

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
#include <poll.h>
#include <time.h>

#ifdef LIB_REGEX
#include <regex.h>
#endif

#ifdef LIB_FORK
#include <sys/wait.h>
#endif

#ifdef LIB_SHELL
#include <termios.h>
#include <sys/ioctl.h>
#endif

#ifdef LIB_MYSQL
#include <mysql/mysql.h>
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

#define STACK 1024
#define MAXTOKEN 4096

enum {
	// codewords
	BYE=1, EXIT, ARG, TAIL, GOTO, DIE,

	IDX, LEAVE, CONT, WHILE, UNTIL, DUP, DROP, OVER, SWAP, PUSH, POP,
	TOP, NIP, ROT, TUCK, SMY, MY, SAT, AT, ATSP, ATFP, ATCSP, ATCFP, FETCH,
	STORE, PSTORE, CFETCH, CSTORE, CELL, NEG, ADD, SUB, SHL, SHR, MUL, DIV,
	MOD, AND, OR, XOR, LESS, MORE, EQUAL, NEQUAL, ZEQUAL, ZLESS, ZMORE, MAX,
	MIN, ADD1, SUB1, SHL1, SHR1, EXECUTE, MOVE, CMOVE, NUMBER, MACRO, NORMAL,
	VALUE, VARY, CREATE, EMIT, KEY, KEYQ, ALLOCATE, RESIZE, FREE, FORMAT,
	DEPTH, HERE, ALLOT, COMMA, CCOMMA, CELLS, BYTES, ZNE, PLACE, PICK, ABS,
	SMOD, TYPE, EVALUATE, COUNT, COMPARE, GETENV, PUTENV, SLURP, BLURT, CORE,
	SYS, COLON, SCOLON, IF, ELSE, FOR, BEGIN, END, COM1, COM2, RECORD, FIELD,
	DOES, ENTER, DOVAL, DOVAR, DOADD, DODOES, LIT_TOK, LIT_NUM, LIT_STR,
	BRANCH, JUMP, LOOP, ELOOP, MACROS, NORMALS, LATEST, ADDER, HEAD_XT,
	XT_HEAD, XT_NAME, XT_CODE, XT_BODY, XT_LIST, XT_LINK, PARSE, SPARSE, FIND,
	FINDPAIR, COMPILE, NCOMPILE, SCOMPILE, MODE, LABEL, REDOES, ONOK, ONWHAT,
	ONERROR, SOURCE, ERROR, USEC, STATIC, RANDOM,

	OPT_DUP_SAT, OPT_DUP_SMY, OPT_DUP_WHILE, OPT_DUP_UNTIL, OPT_DUP_BRANCH,
	OPT_IDX_ADD, OPT_LIT_NUM_ADD,

#ifdef DEBUG
	SLOW, FAST,
#endif

#ifdef LIB_SHELL
	AT_XY, MAX_XY, UNBUFFERED, BUFFERED, PTY_ON, PTY_OFF,
#endif

#ifdef LIB_REGEX
	MATCH, SPLIT,
#endif

#ifdef LIB_FORK
	FORK, SELF, SYSTEM,
#endif

#ifdef LIB_MYSQL
	MYSQL_CONNECT, MYSQL_CLOSE, MYSQL_QUERY, MYSQL_FETCH, MYSQL_ESCAPE,
	MYSQL_FIELDS, MYSQL_FREE,
#endif

	NOP, LASTTOKEN
};

typedef struct {
	tok token;
	char *name;
	tok *high;
} wordinit;

wordinit list_normals[] = {
	{ .token = BYE,      .name = "bye"      },
	{ .token = ARG,      .name = "arg"      },
	{ .token = EXIT,     .name = "exit"     },
	{ .token = DIE,      .name = "die"      },
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
	{ .token = SMY,      .name = "my!"      },
	{ .token = MY,       .name = "my"       },
	{ .token = SAT,      .name = "at!"      },
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
	{ .token = MOVE,     .name = "move"     },
	{ .token = CMOVE,    .name = "cmove"    },
	{ .token = NUMBER,   .name = "number"   },
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
	{ .token = USEC,     .name = "usec"     },
	{ .token = FIELD,    .name = "field"    },
	{ .token = EVALUATE, .name = "evaluate" },
	{ .token = RANDOM,   .name = "random"   },

#ifdef DEBUG
	{ .token = SLOW,     .name = "slow"     },
	{ .token = FAST,     .name = "fast"     },
#endif

#ifdef LIB_SHELL
	{ .token = AT_XY,    .name = "at-xy"    },
	{ .token = MAX_XY,   .name = "max-xy"   },
#endif

#ifdef LIB_REGEX
	{ .token = MATCH,    .name = "match"    },
	{ .token = SPLIT,    .name = "split"    },
#endif

#ifdef LIB_FORK
	{ .token = FORK,     .name = "fork"     },
	{ .token = SELF,     .name = "self"     },
	{ .token = SYSTEM,   .name = "system"   },
#endif

#ifdef LIB_MYSQL
	{ .token = MYSQL_CONNECT,   .name = "mysql_connect"   },
	{ .token = MYSQL_CLOSE,     .name = "mysql_close"     },
	{ .token = MYSQL_QUERY,     .name = "mysql_query"     },
	{ .token = MYSQL_FETCH,     .name = "mysql_fetch"     },
	{ .token = MYSQL_FIELDS,    .name = "mysql_fields"    },
	{ .token = MYSQL_ESCAPE,    .name = "mysql_escape"    },
	{ .token = MYSQL_FREE,      .name = "mysql_free"      },
#endif
};

wordinit list_macros[] = {
	{ .token = MACRO,    .name = "macro"    },
	{ .token = NORMAL,   .name = "normal"   },
	{ .token = COLON,    .name = ":"        },
	{ .token = SCOLON,   .name = ";"        },
	{ .token = IF,       .name = "if"       },
	{ .token = ELSE,     .name = "else"     },
	{ .token = FOR,      .name = "for"      },
	{ .token = BEGIN,    .name = "begin"    },
	{ .token = END,      .name = "end"      },
	{ .token = COM1,     .name = "\\"       },
	{ .token = COM2,     .name = "("        },
	{ .token = RECORD,   .name = "record"   },
	{ .token = DOES,     .name = "does"     },
	{ .token = STATIC,   .name = "static"   },
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

	{ .token = UNBUFFERED, .name = "unbuffered" },
	{ .token = BUFFERED,   .name = "buffered"   },
	{ .token = PTY_ON,     .name = "pseudo-terminal" },
	{ .token = PTY_OFF,    .name = "default-stdio"   },

	{ .token = OPT_DUP_SMY,     .name = "dup_my!"   },
	{ .token = OPT_DUP_WHILE,   .name = "dup_while" },
	{ .token = OPT_DUP_UNTIL,   .name = "dup_until" },
	{ .token = OPT_IDX_ADD,     .name = "i+"        },
	{ .token = OPT_LIT_NUM_ADD, .name = "n+"        },
	{ .token = OPT_DUP_BRANCH,  .name = "?if"       },
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
	if (!file) return NULL;

	unsigned int read = 0;
	for (;;)
	{
		read = fread(pad+len, 1, 1024, file);
		len += read; lim += 1024; pad[len] = 0;
		if (read < 1024) break;
		pad = realloc(pad, lim+1);
	}
	pad[len] = 0;
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

tok *compile_last;
tok *ncompile_last;

// Compile an execution token to code space
void
compile(tok n, tok **p)
{
	tok *cp = *p;
	if (compile_last == cp-1)
	{
		switch (*compile_last)
		{
			case DUP:
				switch (n)
				{
					// dup at!
					case SAT:
						*compile_last = OPT_DUP_SAT;
						return;

					// dup my!
					case SMY:
						*compile_last = OPT_DUP_SMY;
						return;

					// dup while
					case WHILE:
						*compile_last = OPT_DUP_WHILE;
						return;

					// dup until
					case UNTIL:
						*compile_last = OPT_DUP_UNTIL;
						return;

					// dup if
					case BRANCH:
						*compile_last = OPT_DUP_BRANCH;
						return;
				}
				break;

			case SAT:
				switch (n)
				{
					// at! at
					case AT:
						*compile_last = OPT_DUP_SAT;
						return;
				}
				break;

			case SMY:
				switch (n)
				{
					// my! my
					case MY:
						*compile_last = OPT_DUP_SMY;
						return;
				}
				break;

			case IDX:
				switch (n)
				{
					// i +
					case ADD:
						*compile_last = OPT_IDX_ADD;
						return;
				}

		}
	}
	if (compile_last == (tok*)(((char*)cp) - sizeof(cell) - sizeof(tok)) && *compile_last == LIT_NUM)
	{
		switch (n)
		{
			// <n> +
			case ADD:
				*compile_last = OPT_LIT_NUM_ADD;
				return;
		}
	}
	compile_last = cp;
	*cp++ = n;
	*p = cp;
}

// Compile a number to code space
void
ncompile(cell n, tok **p)
{
	tok *cp = *p;
	ncompile_last = cp;
	*((cell*)cp) = n;
	cp = (tok*)(((char*)cp) + sizeof(cell));
	*p = cp;
}


// Compile a counted string to code space
void
scompile(char *s, tok **p)
{
	tok *cp = *p;
	// reserve byte for count
	tok *tokp = cp++;
	char *d = (char*)cp;
	// compile string + null terminator
	while (*s) *d++ = *s++; *d++ = 0;
	// align code-space pointer afterwards
	while ((d - (char*)cp) % sizeof(tok)) *d++ = 0;
	cp = (tok*)d;
	// update count inclusive of alignment bytes
	*tokp = ((char*)cp) - ((char*)tokp);
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
	*tokp = (char*)cp - (char*)tokp;
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

	int len = 0; char c, *s = (char*)source, e = *s++;
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
			if (c == 'a')
				c = '\a';
			else
			if (c == 't')
				c = '\t';
			else
			if (c == '\\')
				c = '\\';
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
	compile_last = NULL;
	ncompile_last = NULL;
	return w-head;
}

int format_i;
#define FORMAT_BUF 1024*64
#define FORMAT_BUFS 3
char *format_bufs[FORMAT_BUFS];

// Format a string using C-like printf() syntax
char*
format(char *in, cell **_dsp)
{
	cell *dsp = *_dsp;
	char *buf = malloc(FORMAT_BUF);

	int idx = format_i++;
	if (format_i == FORMAT_BUFS) format_i = 0;

	char tmp[32], *p;
	int len = 0;

	while (*in && len < FORMAT_BUF-2)
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
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, (char*)dpop);
				}
				else
				if (strchr("cdiouxX", c))
				{
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, dpop);
				}
				else
				if (strchr("eEfgG", c))
				{
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, (double)dpop);
				}
				continue;
			}
		}
		buf[len++] = c;
	}
	buf[len] = 0;
	free(format_bufs[idx]);
	format_bufs[idx] = buf;
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
		// detect outer:inner word notation
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

// Find an object.method pair in a wordlist
tok
findpair(word *w, char *name, tok *xt)
{
	cell a = 0, b = 0;

	char *split = NULL;
	// detect object.method notation
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

FILE *stdin_current;
FILE *stdout_current;

#ifdef LIB_SHELL

void
at_xy(int x, int y)
{
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	x++; y++;

	char tmp[32];
	sprintf(tmp, "\e[%d;%dH", y, x);
	write(fileno(stdout_current), tmp, strlen(tmp));
}

void
max_xy(int *x, int *y)
{
	struct winsize ws;
	ioctl(fileno(stdin_current), TIOCGWINSZ, &ws);
	*x = ws.ws_col-1;
	*y = ws.ws_row-1;
}

#endif

int
key()
{
#ifdef LIB_SHELL
	char c = 0;
	int bytes = read(fileno(stdin_current), &c, 1);
	if (bytes < 1) c = 0;
	return c;
#else
	return 0;
#endif
}

int
keyq()
{
#ifdef LIB_SHELL
	struct pollfd fds;
	fds.fd = fileno(stdin_current);
	fds.events = POLLIN;
	fds.revents = 0;
	return poll(&fds, 1, 0) > 0 ? -1:0;
#else
	return 0;
#endif
}

#ifdef LIB_REGEX

#define REGEX_CACHE 4
char *re_patterns[REGEX_CACHE];
regex_t re_compiled[REGEX_CACHE];

regex_t*
regex(char *pattern)
{
	regex_t *re = NULL; int i;

	// have we compiled this regex before?
	for (i = 0; i < REGEX_CACHE; i++)
	{
		if (re_patterns[i] && same_name(pattern, re_patterns[i]))
		{
			re = &re_compiled[i];
			break;
		}
	}
	if (!re)
	{
		// look for a free cache slot
		for (i = 0; i < REGEX_CACHE; i++)
		{
			if (!re_patterns[i])
				break;
		}
		if (i == REGEX_CACHE)
		{
			// free the oldest
			i = REGEX_CACHE - 1;
			regfree(&re_compiled[0]); free(re_patterns[0]);
			memmove(&re_compiled[0], &re_compiled[1], sizeof(regex_t) * i);
			memmove(&re_patterns[0], &re_patterns[1], sizeof(char*)   * i);
		}
		re = &re_compiled[i];
		re_patterns[i] = strdup(pattern);
		if (regcomp(re, pattern, REG_EXTENDED) != 0)
		{
			re = NULL;
			re_patterns[i] = NULL;
		}
	}
	return re;
}

// POSIX regex match
int
match(char *pattern, char **subject)
{
	int r = 0; regmatch_t pmatch;
	regex_t *re = regex(pattern);
	if (re && subject && *subject)
	{
		r = regexec(re, *subject, 1, &pmatch, 0) == 0 ?-1:0;
		*subject += r ? pmatch.rm_so: strlen(*subject);
	}
	return r;
}

// POSIX regex split
int
split(char *pattern, char **subject)
{
	int r = 0; regmatch_t pmatch;
	regex_t *re = regex(pattern);
	if (re && *subject)
	{
		r = regexec(re, *subject, 1, &pmatch, 0) == 0 ?-1:0;
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
	return r;
}

#endif

#ifdef LIB_FORK

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

#define EXEC_READ 0
#define EXEC_WRITE 1

// execute sub-process and connect its stdin=infp and stdout=outfp
pid_t
exec_cmd_io(const char *command, int *infp, int *outfp)
{
	signal(SIGCHLD, catch_exit);
	int p_stdin[2], p_stdout[2];
	pid_t pid;

	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
		return -1;
	pid = fork();
	if (pid < 0)
		return pid;
	else if (pid == 0)
	{
		close(p_stdin[EXEC_WRITE]);
		dup2(p_stdin[EXEC_READ], EXEC_READ);
		close(p_stdout[EXEC_READ]);
		dup2(p_stdout[EXEC_WRITE], EXEC_WRITE);
		execlp("/bin/sh", "sh", "-c", command, NULL);
		exit(EXIT_FAILURE);
	}
	if (infp == NULL)
		close(p_stdin[EXEC_WRITE]);
	else
		*infp = p_stdin[EXEC_WRITE];
	if (outfp == NULL)
		close(p_stdout[EXEC_READ]);
	else
		*outfp = p_stdout[EXEC_READ];
	close(p_stdin[EXEC_READ]);
	close(p_stdout[EXEC_WRITE]);
	return pid;
}

pid_t
exec_cmd(const char *cmd)
{
	pid_t pid;
	signal(SIGCHLD, catch_exit);
	pid = fork();
	if (!pid)
	{
		setsid();
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}
	return pid;
}

char*
sys_exec(const char *cmd, const char *data)
{
	int in, out;
	exec_cmd_io(cmd, &in, &out);

	if (data)
		write(in, data, strlen(data));
	close(in);

	int len = 0;
	char *res = malloc(1024);
	for (;;)
	{
		int rc = read(out, res+len, 1023);
		if (rc > 0) len += rc;
		if (rc < 1023) break;
		res = realloc(res, len+1024);
	}
	res[len] = 0;
	close(out);
	return res;
}

#endif

#ifdef LIB_MYSQL

MYSQL*
// ( dsn -- handle )
lib_mysql_connect(char *dsn)
{
	int lusername, lpassword, lhostname, lport;

	char *mysqldesc = dsn+8;
	char *susername = mysqldesc;
	char *shostname = strchr(susername, '@'); if (shostname) shostname++;
	char *spassword = strchr(susername, ':'); if (spassword) spassword++;

	// allow blank :password
	if (!spassword || spassword > shostname) {
		spassword = shostname;
		lpassword = 0;
	} else {
		lpassword = shostname - spassword - 1;
	}

	char *sport  = strchr(shostname, ':'); if (sport) sport++;
	char *dbname = strchr(shostname, '/'); if (dbname) dbname++;

	// allow blank :port
	if (!sport) {
		sport = dbname;
		lport = 0;
	} else {
		lport = dbname - sport - 1;
	}

	lusername = spassword - susername - 1;
	lhostname = sport     - shostname - 1;

	char *username  = malloc(lusername+1);
	char *password  = malloc(lpassword+1);
	char *hostname  = malloc(lhostname+1);

	strncpy(username, susername, lusername);
	username[lusername] = '\0';
	strncpy(password, spassword, lpassword);
	password[lpassword] = '\0';
	strncpy(hostname, shostname, lhostname);
	hostname[lhostname] = '\0';

	unsigned int port = 3306;
	if (lport)
	{
		char bport[6];
		strncpy(bport, sport, lport);
		bport[lport] = '\0';
		port = atoi(bport);
	}

	fprintf(stderr, "host: %s, user: %s, pass: %s, database: %s, port: %d", hostname, username, password, dbname, port);

	MYSQL *mysql = mysql_init(NULL);

	if (!mysql_real_connect(mysql, hostname, username, password, dbname, port, NULL, 0))
	{
		fprintf(stderr, "mysql_real_connect: %s", mysql_error(mysql));
		mysql = NULL;
	}
	else
	if (mysql_set_character_set(mysql, "utf8") != 0)
	{
		fprintf(stderr, "mysql_set_character_set: %s", mysql_error(mysql));
		mysql_close(mysql);
		mysql = NULL;
	}

	free(username);
	free(password);
	free(hostname);

	return mysql;
}

MYSQL_RES*
// ( sql -- selected|affected rs )
lib_mysql_query(MYSQL *mysql, char *sql, unsigned long long *rows)
{
	*rows = 0;

	if (mysql_real_query(mysql, sql, strlen(sql)) != 0)
	{
		fprintf(stderr, "mysql_real_query: %s", mysql_error(mysql));
		return 0;
	}

	MYSQL_RES *res = mysql_store_result(mysql);

	if (!res)
	{
		// could be statement without results, eg INSERT
		if (mysql_field_count(mysql) != 0)
		{
			fprintf(stderr, "mysql_store_result: %s", mysql_error(mysql));
			return 0;
		}
		*rows = mysql_affected_rows(mysql);
		return NULL;
	}
	else
	{
		*rows = mysql_num_rows(res);
		return res;
	}
}

unsigned char*
// ( rs -- addr )
lib_mysql_fetch(MYSQL_RES *res, unsigned long long index)
{
	if (index > mysql_num_rows(res))
	{
		fprintf(stderr, "invalid mysql result index: %lld", index);
		return NULL;
	}

	mysql_data_seek(res, index);
	MYSQL_ROW row = mysql_fetch_row(res);

	unsigned int cols = mysql_num_fields(res);
	unsigned long *lengths = mysql_fetch_lengths(res);
	MYSQL_FIELD *fields = mysql_fetch_fields(res);

	int len = 0;
	unsigned char *data = calloc(1024, 1);

	int i;
	for (i = 0; i < cols; i++)
	{
		data = realloc(data, len + lengths[i] + sizeof(cell) + 2);

		unsigned char *flags = data + len;
		data[len++] = 0;

		*((cell*)(data + len)) = lengths[i];
		len += sizeof(cell);

		if (!row[i])
			*flags |= 1; // null
		else
		{
			memmove(data + len, row[i], lengths[i]);
			len += lengths[i];
		}

		if (fields[i].type == MYSQL_TYPE_BLOB)
			*flags |= 2;

		data[len++] = 0;
	}
	return data;
}

char*
lib_mysql_fields(MYSQL_RES *res, unsigned int *cols)
{
	*cols = mysql_num_fields(res);
	MYSQL_FIELD *fields = mysql_fetch_fields(res);

	int len = 0;
	char *data = calloc(1024, 1);

	int i;
	for (i = 0; i < *cols; i++)
	{
		int flen = strlen(fields[i].name);
		data = realloc(data, len + flen+1);
		strcpy(data + len, fields[i].name);
		len += flen+1;
	}
	return data;
}

#endif

tok init[] = { EVALUATE, BYE };

#include "src_base.c"

#ifdef TURNKEY
#include "src_turnkey.c"
#endif

// Use GCC's &&label syntax to find code word adresses.
#define CODE(x) call[(x)] = &&code_##x; if (0) { code_##x:

// Framework to execute a Forth word from inside a CODE block (mainly for EVALUATE)
#define IEXECUTE(x,l) do { iexec[0] = (x); iexec[1] = GOTO; *rsp++ = (cell)&&iexec_##l; *rsp++ = (cell)ip; ip = iexec; INEXT } while(0); iexec_##l:

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
	cell ds[STACK]; // Data stack
	cell rs[STACK]; // Return stack
	cell as[STACK]; // Alternate data stack (PUSH, POP, TOP)
	cell ls[STACK]; // Loop stack

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
	cell *lsp; // Loop stack pointer

	int i, j;
	tok *tokp, exec[2], iexec[2], *cp, xt1, xt2;
	cell *cellp, tmp, tos, num;
	char *charp, *s1, *s2, c;
	cell index, limit, src, dst;
	word *w, *hp;
	void *voidp;

	srand(time(0));

#define RSP_MY  -1
#define RSP_AT  -2
#define RSP_LSP -3
#define RSP_IP  -4
#define RSP_NEST 4

#define LSP_IDX -1
#define LSP_LIM -2
#define LSP_IP  -3
#define LSP_NEST 3

#define ip_jmp ip = (tok*)(((char*)ip) + *ip)

#ifdef DEBUG
	int single = 0;
#endif

#ifdef LIB_SHELL
	int input_state = 0;
	int pty_state = 0;
	struct termios old_tio, new_tio;
	stdin_current = stdin;
	stdout_current = stdout;
#endif

#ifdef LIB_MYSQL
	unsigned long long rows;
#endif

	// Initialize the dictionary headers

	word *last = NULL;

	hp = &head[CORE];
	word *core = hp;
	hp->name = "core";
	call[hp-head] = &&code_NOP;
	hp->subs = hp;
	hp->prev = last;
	last = hp;
	hp++;

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

	core->subs = last;

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
	lsp = ls+3;
	ip = init;

	char *fsrc = strdup(src_base);

#ifdef TURNKEY

	fsrc = realloc(fsrc, strlen(fsrc) + strlen(src_turnkey) + 2);
	strcat(fsrc, "\n");
	strcat(fsrc, src_turnkey);

#else

	// Assume the last command line argument is our Forth source file
	char *run = NULL;

	for (i = 1; i < argc; i++)
	{
		if (!run && strncmp(argv[i], "--", 2))
		{
			run = argv[i];
		}
		else
		if (!strcmp(argv[i], "--dump-envs"))
		{
			for (j = 0; env[j]; j++)
				fprintf(stderr, "%s\n", env[j]);
		}
	}
	if (run)
	{
		char *pad = slurp(run);
		if (pad)
		{
			fsrc = realloc(fsrc, strlen(fsrc) + 2 + strlen(pad));
			strcat(fsrc, "\n");
			strcat(fsrc, pad);
		}
		free(pad);
	}
	else
	{
		fsrc = realloc(fsrc, strlen(fsrc) + 50);
		strcat(fsrc, "\nshell");
	}

#endif

	// First word called is always EVALUATE, so place source address TOS
	tos = (cell)fsrc;

	// Start code word labels

	// These *must* come before the first INEXT executes because CODE
	// generates code to initialize the call[] array at runtime...

	// ( -- )
	CODE(BYE)
		tos = 0;
		goto shutdown;
	NEXT

	CODE(DIE)
		goto shutdown;
	NEXT

	CODE(ARG)
		tos = tos < argc && tos >= 0 ? (cell)(argv[tos]): 0;
	NEXT

	// ( -- )
	CODE(ENTER)
		rsp += RSP_NEST;
		rsp[RSP_IP]  = (cell)ip;
		rsp[RSP_LSP] = (cell)lsp;
		ip = body[xt];
	NEXT

	// ( -- )
	CODE(EXIT)
		ip = (tok*)rsp[RSP_IP];
		lsp = (cell*)rsp[RSP_LSP];
		rsp -= RSP_NEST;
	NEXT

	// ( xt -- )
	CODE(EXECUTE)
		xt = tos;
		tos = dpop;
		if (xt)
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
		rsp += RSP_NEST;
		rsp[RSP_IP]  = (cell)ip;
		rsp[RSP_LSP] = (cell)lsp;
		dpush(tos);
		tos = ((cell*)body[xt])[0];
		ip = (tok*)((cell*)body[xt])[1];
	NEXT

	// ( -- )
	CODE(TAIL)
		ip = body[*ip];
	NEXT

	// ( -- )
	CODE(GOTO)
		ip = (tok*)(*--rsp);
		voidp = (void*)(*--rsp);
		goto *voidp;
	NEXT

#ifdef DEBUG
	// ( -- )
	CODE(SLOW)
		single = 1;
	NEXT

	// ( -- )
	CODE(FAST)
		single = 0;
	NEXT
#endif

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
	CODE(SMY)
		rsp[RSP_MY] = tos;
		tos = dpop;
	NEXT

	// ( n -- n )
	CODE(OPT_DUP_SMY)
		rsp[RSP_MY] = tos;
	NEXT

	// ( -- n )
	CODE(MY)
		dpush(tos);
		tos = rsp[RSP_MY];
	NEXT

	// ( a -- )
	CODE(SAT)
		rsp[RSP_AT] = tos;
		tos = dpop;
	NEXT

	// ( a -- a )
	CODE(OPT_DUP_SAT)
		rsp[RSP_AT] = tos;
	NEXT

	// ( -- a )
	CODE(AT)
		dpush(tos);
		tos = rsp[RSP_AT];
	NEXT

	// ( n -- )
	CODE(ATSP)
		cellp = (cell*)(rsp[RSP_AT]);
		*cellp++ = tos;
		rsp[RSP_AT] = (cell)cellp;
		tos = dpop;
	NEXT

	// ( -- n )
	CODE(ATFP)
		dpush(tos);
		cellp = (cell*)(rsp[RSP_AT]);
		tos = *cellp++;
		rsp[RSP_AT] = (cell)cellp;
	NEXT

	// ( c -- )
	CODE(ATCSP)
		charp = (char*)(rsp[RSP_AT]);
		*charp++ = tos;
		rsp[RSP_AT] = (cell)charp;
		tos = dpop;
	NEXT

	// ( -- c )
	CODE(ATCFP)
		dpush(tos);
		charp = (char*)(rsp[RSP_AT]);
		tos = *charp++;
		rsp[RSP_AT] = (cell)charp;
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

	// ( -- n )
	CODE(OPT_LIT_NUM_ADD)
		tos += *((cell*)ip);
		ip = (tok*)(((char*)ip) + sizeof(cell));
	NEXT

	// ( -- a )
	CODE(LIT_STR)
		dpush(tos);
		tos = (cell)(ip+1);
		ip_jmp;
	NEXT

	// ( f -- )
	CODE(BRANCH)
		if (tos) ip++;
		else ip_jmp;
		tos = dpop;
	NEXT

	// ( f -- )
	CODE(OPT_DUP_BRANCH)
		if (tos) ip++;
		else ip_jmp;
	NEXT

	// ( -- )
	CODE(JUMP)
		ip_jmp;
	NEXT

	// ( n -- )
	CODE(LOOP)
		if (tos)
		{
			lsp += LSP_NEST;
			lsp[LSP_IP]  = (cell)ip; // break
			lsp[LSP_LIM] = tos; // limit
			lsp[LSP_IDX] = 0; // index
			ip++;
		}
		else
		{
			ip_jmp;
		}
		tos = dpop;
	NEXT

	// ( -- )
	CODE(ELOOP)
		index = lsp[LSP_IDX];
		limit = lsp[LSP_LIM];
		if (++index < limit || limit < 0)
		{
			ip = (tok*)(lsp[LSP_IP]);
			lsp[LSP_IDX] = index;
			ip++;
		}
		else
		{
			lsp -= LSP_NEST;
		}
	NEXT

	// ( -- i )
	CODE(IDX)
		dpush(tos);
		tos = lsp[LSP_IDX];
	NEXT

	// ( -- i )
	CODE(OPT_IDX_ADD)
		tos += lsp[LSP_IDX];
	NEXT

	// ( -- )
	CODE(LEAVE)
		ip = (tok*)(lsp[LSP_IP]);
		lsp -= LSP_NEST;
		ip_jmp;
	NEXT

	// ( -- )
	CODE(CONT)
		ip = (tok*)(lsp[LSP_IP]);
		ip_jmp; ip--;
	NEXT

	// ( f -- )
	CODE(WHILE)
		tmp = tos;
		tos = dpop;
		if (!tmp)
			goto code_LEAVE;
	NEXT

	// ( f -- f )
	CODE(OPT_DUP_WHILE)
		if (!tos)
			goto code_LEAVE;
	NEXT

	// ( f -- )
	CODE(UNTIL)
		tmp = tos;
		tos = dpop;
		if (tmp)
			goto code_LEAVE;
	NEXT

	// ( f -- f )
	CODE(OPT_DUP_UNTIL)
		if (tos)
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
	CODE(MOVE)
		dst = dpop, src = dpop;
		if (tos > 0)
			memmove((char*)dst, (char*)src, tos * sizeof(cell));
		tos = dpop;
	NEXT

	// ( src dst len -- )
	CODE(CMOVE)
		dst = dpop, src = dpop;
		if (tos > 0)
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

	// ( s1 s2 n -- f )
	CODE(COMPARE)
		s2 = (char*)dpop;
		s1 = (char*)dpop;
		tmp = tos;
		if (!s1 && !s2)
			tos = 0;
		else
		if (s1 && !s2)
			tos = 1;
		else
		if (!s1 && s2)
			tos = -1;
		else
		if (tmp)
			tos = strncmp(s1, s2, tmp);
		else
			tos = strcmp(s1, s2);
	NEXT

	// ( c -- )
	CODE(EMIT)
		c = tos;
		tos = dpop;
		write(fileno(stdout_current), &c, 1);
	NEXT

	// ( -- c )
	CODE(KEY)
		dpush(tos);
		tos = key();
	NEXT

	// ( -- f )
	CODE(KEYQ)
		dpush(tos);
		tos = keyq();
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
		cp = (tok*)(((char*)cp) + tos);
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
		if (tmp)
			memmove((char*)tos, (char*)tmp, strlen((char*)tmp)+1);
		else
			*((char*)tos) = 0;
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
		dpush(current == &normal);
		compile(JUMP, &cp);
		dpush((cell)mark(&cp));
		xt = label(&hp);
		call[xt] = &&code_ENTER;
		body[xt] = cp;
		tos = xt;
		// Sub-words default to normals
		current = &normal;
	NEXT

	// ( a xt -- )
	CODE(SCOLON)
		// tail recursion
		if (compile_last == cp-1 && call[*compile_last] == &&code_ENTER)
		{
			*cp++ = *compile_last;
			cp[-2] = TAIL;
		}
		else
		{
			compile(EXIT, &cp);
		}
		mode--;
		patch((tok*)dpop, &cp);
		// Normal sub-words are externaly accessible
		head[tos].subs = normal;
		// Macros sub-words are not externally accessible
		while (macro > normal)
			macro = macro->prev;
		current = dpop ? &normal: &macro;
		*current = &head[tos];
		tos = dpop;
		compile_last  = NULL;
		ncompile_last = NULL;
	NEXT

	// ( -- a n )
	CODE(IF)
		dpush(tos);
		compile(BRANCH, &cp);
		if (!mode)
			dpush((cell)(cp-1));
		dpush((cell)mark(&cp));
		mode++;
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
			dpush(-1);
			dpush((cell)cp);
		}
		mode++;
		compile(LOOP, &cp);
		dpush((cell)mark(&cp));
		tos = 2;
	NEXT

	// ( ... a n -- )
	CODE(END)
		compile_last  = NULL;
		ncompile_last = NULL;
		// IF or ELSE
		if (tos == 1)
		{
			patch((tok*)dpop, &cp);
		}
		else
		// BEGIN or FOR
		if (tos == 2)
		{
			compile(ELOOP, &cp);
			patch((tok*)dpop, &cp);
		}
		else
		// RECORD
		if (tos == 3)
		{
			tmp = dpop; // size
			tos = dpop; // xt
			patch((tok*)dpop, &cp);
			mode = dpop;
			dpush(tmp);
			goto code_VARY;
		}
		// IF BEGIN FOR enter compile mode automatically if called while in interpret mode.
		// END detects that, auto-executes the code fragment, and reverts to interpret mode.
		if (tos == 1 || tos == 2)
		{
			mode--;
			if (!mode)
			{
				compile(EXIT, &cp);
				rsp += RSP_NEST;
				rsp[RSP_IP]  = (cell)ip;
				rsp[RSP_LSP] = (cell)lsp;
				ip = (tok*)dpop;
			}
		}
		tos = dpop;
	NEXT

	// ( -- f a xt 0 n )
	CODE(RECORD)
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
	CODE(STATIC)
		goto code_RECORD;
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
		if (tos)
			write(fileno(stdout_current), (char*)tos, strlen((char*)tos));
		tos = dpop;
	NEXT

	// ( a -- )
	CODE(ERROR)
		write(fileno(stderr), (char*)tos, strlen((char*)tos));
		tos = dpop;
	NEXT

	// ( n -- )
	CODE(USEC)
		usleep(tos);
		tos = dpop;
	NEXT

	// ( n -- r )
	CODE(RANDOM)
		tos = rand() % tos;
	NEXT

#ifdef LIB_SHELL

	// ( x y -- )
	CODE(AT_XY)
		at_xy(dpop, tos);
		tos = dpop;
	NEXT

	// ( -- x y )
	CODE(MAX_XY)
		dpush(tos);
		max_xy(&i, &j);
		dpush(i);
		tos = j;
	NEXT

	CODE(UNBUFFERED)
		if (!input_state)
		{
			tcgetattr(fileno(stdin_current),&old_tio);
			new_tio = old_tio;
			new_tio.c_lflag &= (~ICANON & ~ECHO);
			new_tio.c_cc[VTIME] = 0;
			new_tio.c_cc[VMIN] = 1;
			tcsetattr(fileno(stdin_current), TCSANOW, &new_tio);
			input_state = 1;
		}
	NEXT

	CODE(PTY_ON)
		if (!pty_state)
		{
			stdin_current = fopen("/dev/tty", "r");
			stdout_current = fopen("/dev/tty", "w");
			write(fileno(stdout_current), "\e[?1049h", 8);
			pty_state = 1;
		}
	NEXT

	CODE(BUFFERED)
		if (input_state)
		{
			tcsetattr(fileno(stdin_current), TCSANOW, &old_tio);
			input_state = 0;
		}
	NEXT

	CODE(PTY_OFF)
		if (pty_state)
		{
			write(fileno(stdout_current), "\e[?1049l", 8);
			fclose(stdin_current);
			fclose(stdout_current);
			stdin_current = stdin;
			stdout_current = stdout;
			pty_state = 0;
		}
	NEXT

#endif

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

	// ( data cmd -- res )
	CODE(SYSTEM)
		tmp = dpop;
		tos = (cell)sys_exec((char*)tos, tmp ? (char*)tmp: "");
	NEXT

#endif

#ifdef LIB_MYSQL

	// ( dsn -- handle )
	CODE(MYSQL_CONNECT)
		tos = (cell)lib_mysql_connect((char*)tos);
	NEXT

	// ( handle -- )
	CODE(MYSQL_CLOSE)
		if (tos)
			mysql_close((MYSQL*)tos);
		tos = dpop;
	NEXT

	// ( sql handle -- handle selected|affected )
	CODE(MYSQL_QUERY)
		tmp = (cell)lib_mysql_query((MYSQL*)tos, (char*)dpop, &rows);
		dpush((cell)tmp);
		tos = rows;
	NEXT

	// ( rhandle -- addr )
	CODE(MYSQL_FETCH)
		tos = (cell)lib_mysql_fetch((MYSQL_RES*)dpop, (unsigned long long)tos);
	NEXT

	// ( rhandle -- addr num )
	CODE(MYSQL_FIELDS)
		dpush((cell)lib_mysql_fields((MYSQL_RES*)tos, (unsigned int*)&rows));
		tos = rows;
	NEXT

	// ( src dst len handle -- len' )
	CODE(MYSQL_ESCAPE)
		s1 = (char*)dpop;
		s2 = (char*)dpop;
		tmp = dpop;
		tos = mysql_real_escape_string((MYSQL*)tos, s1, s2, tmp);
	NEXT

	// ( rhandle -- )
	CODE(MYSQL_FREE)
		if (tos)
			mysql_free_result((MYSQL_RES*)tos);
		tos = dpop;
	NEXT

#endif

	CODE(EVALUATE)
		*asp++ = source;
		source = tos;
		tos = dpop;
		if (source) for (;;)
		{
			// stack underflow check
			if (dsp < ds+2)
			{
				dsp = ds+3;
				tos = 0;
				if (on_error)
				{
					dpush(tos);
					tos = 1;
					IEXECUTE(on_error, eval0)
					if (!tos)
					{
						tos = dpop;
						goto evaluate_end;
					}
					tos = dpop;
				}
				else
				{
					goto evaluate_end;
				}
			}

			if (!((charp = parse()) && *charp))
				break;

			// double-quoted string literal
			if (*charp == '"')
			{
				source -= strlen(charp);
				charp = sparse();
				if (mode)
				{
					compile(LIT_STR, &cp);
					scompile(charp, &cp);
				}
				else
				{
					dpush(tos);
					tos = (cell)charp;
				}
				continue;
			}
			// immediate word
			if ((xt = find(macro, charp)))
			{
				IEXECUTE(xt, eval1)
				continue;
			}
			// normal word
			if ((xt = find(normal, charp)))
			{
				if (mode)
				{
					compile(xt, &cp);
					continue;
				}
				IEXECUTE(xt, eval2)
				continue;
			}
			// literal
			if (number(charp, &tmp))
			{
				if (mode)
				{
					compile(LIT_NUM, &cp);
					ncompile(tmp, &cp);
					continue;
				}
				dpush(tos);
				tos = tmp;
				continue;
			}
			// find a word
			if (*charp == '\'')
			{
				if ((xt = find(macro, charp+1)))
				{
					if (mode)
					{
						compile(LIT_TOK, &cp);
						compile(xt, &cp);
					}
					else
					{
						dpush(tos);
						tos = xt;
					}
					continue;
				}
				if ((xt = find(normal, charp+1)))
				{
					if (mode)
					{
						compile(LIT_TOK, &cp);
						compile(xt, &cp);
					}
					else
					{
						dpush(tos);
						tos = xt;
					}
					continue;
				}
			}
			else
			// ascii char literal
			if (*charp == '`')
			{
				if (mode)
				{
					compile(LIT_NUM, &cp);
					ncompile(charp[1], &cp);
				}
				else
				{
					dpush(tos);
					tos = charp[1];
				}
				continue;
			}
			else
			// object.method notation
			if ((xt1 = findpair(normal, charp, &xt2)))
			{
				if (mode)
				{
					compile(xt1, &cp);
					compile(xt2, &cp);
					continue;
				}
				IEXECUTE(xt1, eval3)
				IEXECUTE(xt2, eval4)
				continue;
			}
			// unknown word
			if (on_what)
			{
				dpush(tos);
				tos = (cell)charp;
				IEXECUTE(on_what, eval5)
				if (tos)
				{
					tos = dpop;
					continue;
				}
				goto evaluate_end;
			}
			dpush(tos);
			tos = 0;
			goto evaluate_end;
		}
		// success!
		if (on_ok)
		{
			IEXECUTE(on_ok, eval6)
			tmp++; // gcc complains about IEXECUTE without this NOP
		}
		dpush(tos);
		tos = -1;
	evaluate_end:
		source = *--asp;
	NEXT

	// ( -- )
	CODE(NOP)

	NEXT

#ifdef DEBUG
	next:
		if (dsp < ds+2)
		{
			fprintf(stderr, "stack underflow");
			fprintf(stderr, "\n%s", head[*ip].name);
			exit(EXIT_FAILURE);
		}
		if (single)
		{
			// single step debugger
			fprintf(stderr, "\nD: %lld ", (long long)tos);
			for (i = -1; dsp+i+1 > ds+3 && i > -10; i--)
				fprintf(stderr, "%lld ", (long long)dsp[i]);
			fprintf(stderr, "\nA: ");
			for (i = -1; asp+i+1 > as+3 && i > -10; i--)
				fprintf(stderr, "%lld ", (long long)asp[i]);
			fprintf(stderr, "\n%s", head[*ip].name);
			key();
		}
#endif

	// Finally, jump into Forth!
	INEXT

	shutdown:

		if (input_state)
		{
			tcsetattr(fileno(stdin_current), TCSANOW, &old_tio);
		}
		if (pty_state)
		{
			write(fileno(stdout_current), "\e[?1049l", 8);
		}

	free(fsrc);
	return tos;
}
