// Reforth Parser

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <err.h>
#include <assert.h>

typedef int64_t cell;

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

int
match(char *pattern, char *subject)
{
	regex_t _re, *re = &_re;

	if (regcomp(re, pattern, REG_EXTENDED) != 0)
	{
		fprintf(stderr, "regex compile failure: %s", pattern);
		exit(1);
	}
	int r = 0; regmatch_t pmatch;
	if (subject)
	{
		r = regexec(re, subject, 1, &pmatch, 0) == 0 ?-1:0;
	}
	regfree(re);
	return r;
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

char *src;
char parsed[1024];

// Parse a white-space delimited word from src
char*
parse()
{
	int len = 0;
	char *s = src;
	while (*s && *s < 33) s++;
	while (*s && *s > 32 && len < 1023) parsed[len++] = *s++;
	parsed[len] = 0;
	src = s;
	return len ? parsed: NULL;
}

char sparsed[1024];

// Parse a quote delimited string literal from source
char*
sparse()
{
	char *buf = sparsed;

	int len = 0; char c, *s = src, e = *s++;
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
	src = s;
	return buf;
}

char *scopes[1024];
int scope;

char *proto;
char *local;
char *global;
char *all_proto;
char *all_local;
char *all_global;

char namespace[1024], *words[65535];
int word, cword, vars, rec, links[65535];

char *sources[1024];
int source;

char*
wname(char *name)
{
	int j;
	for (j = strlen(name); j > 0 && name[j] != ':'; j--);
	return name + j + 1;
}

void
open_scope(char *name)
{
	scopes[scope++] = proto;
	scopes[scope++] = local;
	scopes[scope++] = global;

	proto  = calloc(1, 1024);
	local  = calloc(1, 1024);
	global = calloc(1, 1024);

	sprintf(namespace + strlen(namespace), ":%s", name);
	words[++word] = strdup(namespace);
}

void
close_scope()
{
	all_proto  = realloc(all_proto,  strlen(all_proto)  + strlen(proto)  + 1);
	strcat(all_proto, proto);
	all_local  = realloc(all_local,  strlen(all_local)  + strlen(local)  + 1);
	strcat(all_local, local);
	all_global = realloc(all_global, strlen(all_global) + strlen(global) + 1);
	strcat(all_global, global);

	free(proto);
	free(local);
	free(global);

	global = scopes[--scope];
	local  = scopes[--scope];
	proto  = scopes[--scope];

	wname(namespace)[-1] = 0;
}

int
find_name(char *name)
{
	char alias[1024];
	strcpy(alias, namespace);

	int i, levels;
	for (i = 0, levels = 0; alias[i]; i++)
		if (alias[i] == ':') levels++;

	for (i = 0; i < levels; i++)
	{
		char *eol = alias + strlen(alias);
		sprintf(eol, ":%s", name);
		int i;
		for (i = word; i > 0; i--)
		{
			if (!strcmp(words[i], alias))
				return links[i] ? links[i]: i;
		}
		*eol = 0;
		for (i = strlen(alias); i >= 0; i--)
			if (alias[i] == ':')
				break;
		alias[i] = 0;
	}

	return 0;
}

void
pcompile(char *fmt, ...)
{
	proto = realloc(proto, strlen(proto) + 1024);
	va_list ap;
	va_start(ap,fmt);
	vsprintf(proto + strlen(proto), fmt, ap);
	va_end(ap);
}

void
lcompile(char *fmt, ...)
{
	local = realloc(local, strlen(local) + 1024);
	va_list ap;
	va_start(ap,fmt);
	vsprintf(local + strlen(local), fmt, ap);
	va_end(ap);
}

void
gcompile(char *fmt, ...)
{
	global = realloc(global, strlen(global) + 1024);
	va_list ap;
	va_start(ap,fmt);
	vsprintf(global + strlen(global), fmt, ap);
	va_end(ap);
}

int
main (int argc, char *argv[])
{
	cell arg, n, strings = 0;
	all_proto  = calloc(1, 1024);
	all_local  = calloc(1, 1024);
	all_global = calloc(1, 1024);

	int f_format  = 0;
	int f_at_xy   = 0;
	int f_max_xy  = 0;
	int f_match   = 0;
	int f_split   = 0;
	int f_key     = 0;
	int f_keyq    = 0;
	int f_compare = 0;
	int f_slurp   = 0;
	int f_blurt   = 0;
	int f_number  = 0;
	int f_fork    = 0;

	open_scope("_global_");
	lcompile("cell _f_main(cell tos) {\n\tcell at, my, i = 0; do {\n");

	int _stk[1024], *stk = _stk, last = 0;

	for (arg = 1; arg < argc; arg++)
	{
		char *slurped = NULL;

		if (!strcmp("-e", argv[arg]) && arg+1 < argc)
		{
			slurped = strdup(argv[++arg]);
		}
		else
		{
			slurped = slurp(argv[arg]);
		}

		src = slurped;
		while (src && *src)
		{
			while (isspace(*src)) src++;

			if (*src == '"')
			{
				sparse();
				pcompile("static const char string%d[] = {", strings);
				for (n = 0; n < strlen(sparsed); n++)
					pcompile("%d,", sparsed[n]);
				pcompile("0};\n");
				lcompile("\tpush(tos); tos = (cell)string%d;\n", strings);
				strings++;
				continue;
			}

			if (!parse()) break;

			if (!strcmp("\\", parsed))
			{
				while (*src && *src++ != '\n');
				continue;
			}

			if (!strcmp("(", parsed))
			{
				while (*src && *src++ != ')');
				continue;
			}

			if (!strcmp("{", parsed))
			{
				int levels = 1;
				char tmp[2]; tmp[1] = 0;
				lcompile(parsed);
				while (levels > 0 && *src)
				{
					if (*src == '{') levels++;
					if (*src == '}') levels--;
					tmp[0] = *src++;
					lcompile(tmp);
				}
				lcompile("\n");
				continue;
			}

			if (!strcmp(":", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_word%d(cell); // %s\n", word, parsed);
				lcompile("static cell f_word%d(cell tos) { // %s\n\tcell at, my, i = 0; do {\n", word, parsed);
				*stk++ = 1;
				*stk++ = word;
				continue;
			}

			if (!strcmp(":noname", parsed))
			{
				open_scope("_noname");
				pcompile("static cell f_word%d(cell); // (noname)\n", word);
				lcompile("static cell f_word%d(cell tos) { // (noname)\n\tcell at, my, i = 0; do {\n", word);
				*stk++ = 2;
				*stk++ = word;
				continue;
			}

			if (!strcmp(";", parsed))
			{
				n = *--stk;
				lcompile("\t} while(0); end: return tos;\n}\n");
				close_scope();
				if (*--stk == 2)
					lcompile("push(tos); tos = (cell)(&f_word%d);\n", n);
				continue;
			}

			if (!strcmp("value", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_val%d; // %s\n", word, parsed);
				pcompile("static inline cell f_word%d(cell tos) { push(tos); return f_val%d; } // value\n", word, word);
				close_scope();
				lcompile("\tf_val%d = tos; tos = pop; // %s\n", word, parsed);
				continue;
			}

			if (!strcmp("as", parsed))
			{
				assert(parse());
				open_scope(parsed);
				close_scope();
				lcompile("#define f_word%d(t) push(tos) || 1 ? f_val%d: f_val%d; // local %s\n", word, word, word, parsed);
				lcompile("\tcell f_val%d = tos; tos = pop; // %s\n", word, parsed);
				continue;
			}

			if (!strcmp("bind", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_val%d; // %s\n", word, parsed);
				pcompile("static word f_bind%d; // %s\n", word, parsed);
				pcompile("static inline cell f_word%d(cell tos) { push(tos); return f_bind%d(f_val%d); } // bind\n", word, word, word);
				close_scope();
				lcompile("\tf_bind%d = (word)tos; tos = pop; // %s\n", word, parsed);
				lcompile("\tf_val%d = tos; tos = pop; // %s\n", word, parsed);
				continue;
			}

			if (!strcmp("wrap", parsed))
			{
				int i, id = last, nid;
				char name[1024];

				assert(parse());
				open_scope(parsed);
				nid = word;
				pcompile("static cell f_val%d;\n", nid);
				pcompile("static inline cell f_word%d(cell tos) { push(tos); return f_val%d; }\n", nid, nid);
				close_scope();

				lcompile("\ttos = pop;\n"); // last
				lcompile("\tf_val%d = tos; tos = pop; // %s\n", word, parsed);

				for (i = id; i < nid; i++)
				{
					if (words[i] && !strncmp(words[i], words[id], strlen(words[id])) && strcmp(words[i], words[id]))
					{
						char *method = wname(words[i]);
						snprintf(name, 1024, "%s.%s", parsed, method);

						open_scope(name);
						pcompile("static word f_bind%d; // method %s\n", word, method);
						pcompile("static inline cell f_word%d(cell tos) { push(tos); return f_bind%d(f_val%d); }\n",
							word, word, nid);
						close_scope();

						lcompile("\tf_bind%d = f_word%d; // method %s\n", word, i, method);
					}
				}
				continue;
			}

			if (!strcmp("from", parsed))
			{
				int id, i, j, l;
				char copy[1024];

				assert(parse());
				assert((id = find_name(parsed)));

				l = word;
				for (i = id; i < l; i++)
				{
					if (words[i] && !strncmp(words[id], words[i], strlen(words[id])) && strcmp(words[id], words[i]))
					{
						char *method = wname(words[i]);
						snprintf(copy, 1024, "%s:%s", words[n], method);
						words[++word] = strdup(copy);
						links[word] = i;
					}
				}
				continue;
			}

			if (!strcmp("to", parsed))
			{
				assert(parse());
				assert((n = find_name(parsed)));
				lcompile("\tf_val%d = tos; tos = pop; // to %s\n", n, parsed);
				continue;
			}

			if (!strcmp("is", parsed))
			{
				assert(parse());
				assert((n = find_name(parsed)));
				lcompile("\tf_bind%d = (word)tos; tos = pop; // %s\n", n, parsed);
				continue;
			}

			if (!strcmp("array", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_size%d, f_cell%d; // array %s\n", word, word, parsed);
				pcompile("static char *f_addr%d; // array %s\n", word, parsed);
				pcompile("static inline cell f_word%d(cell tos) { // array %s\n", word, parsed);
				pcompile("\tif (tos >= 0 && tos < f_size%d)\n", word);
				pcompile("\t\treturn (cell)(f_addr%d + (tos * f_cell%d));\n", word, word);
				pcompile("\tfprintf(stderr, \"array out of bounds: %s[%%ld] %%ld\\n\", f_size%d, tos);\n\texit(1);\n", parsed, word);
				pcompile("}\n");
				close_scope();
				lcompile("\tf_cell%d = tos; tos = pop;\n", word);
				lcompile("\tf_size%d = tos; tos = pop;\n", word);
				lcompile("\tfree(f_addr%d); f_addr%d = calloc(f_size%d, f_cell%d); // %s\n", word, word, word, word, parsed);
				continue;
			}

			if (!strcmp("field", parsed))
			{
				assert(parse());
				open_scope(parsed);
				close_scope();
				pcompile("static inline cell f_word%d(cell tos) { return tos + %d; } // %s\n", word, vars, parsed);
				vars += sizeof(cell);
				continue;
			}

			if (!strcmp("record", parsed))
			{
				assert(parse());
				open_scope(parsed);
				close_scope();
				pcompile("// record %s\n", parsed);
				rec = word;
				vars = 0;
				continue;
			}

			if (!strcmp("if", parsed))
			{
				lcompile("\t{ cell f = tos; tos = pop; if (f) { // if\n");
				continue;
			}

			if (!strcmp("else", parsed))
			{
				lcompile("\t} else { // else\n");
				continue;
			}

			if (!strcmp("for", parsed))
			{
				lcompile("\t{ cell j = i, i = 0, l = tos; tos = pop; for (; i < l; i++) { // for\n");
				continue;
			}

			if (!strcmp("begin", parsed))
			{
				lcompile("\t{ cell j = i, i = 0; for (; ; i++) { // begin\n");
				continue;
			}

			if (!strcmp("end", parsed))
			{
				if (rec)
				{
					pcompile("static inline cell f_word%d(cell tos) { push(tos); return %d; } // record\n", rec, vars);
					rec = 0;
				}
				else
				{
					lcompile("\t}} // end\n");
				}
				continue;
			}

			if (!strcmp("i", parsed))
			{
				lcompile("\tpush(tos); tos = i; // i\n");
				continue;
			}

			if (!strcmp("j", parsed))
			{
				lcompile("\tpush(tos); tos = j; // j\n");
				continue;
			}

			if (!strcmp("while", parsed))
			{
				lcompile("\t{ cell f = tos; tos = pop; if (!f) break; } // while\n");
				continue;
			}

			if (!strcmp("until", parsed))
			{
				lcompile("\t{ cell f = tos; tos = pop; if (f) break; } // until\n");
				continue;
			}

			if (!strcmp("leave", parsed))
			{
				lcompile("\tbreak; // leave\n");
				continue;
			}

			if (!strcmp("next", parsed))
			{
				lcompile("\tcontinue; // next\n");
				continue;
			}

			if (!strcmp("exit", parsed))
			{
				lcompile("\tgoto end; // exit\n");
				continue;
			}

			// high-level words
			if ((n = find_name(parsed)))
			{
				lcompile("\ttos = f_word%d(tos); // %s\n", n, parsed);
				continue;
			}

			if (!strcmp("at", parsed))
			{
				lcompile("\tpush(tos); tos = at; // at\n");
				continue;
			}

			if (!strcmp("my", parsed))
			{
				lcompile("\tpush(tos); tos = my; // my\n");
				continue;
			}

			if (!strcmp("at!", parsed))
			{
				lcompile("\tat = tos; tos = pop; // at!\n");
				continue;
			}

			if (!strcmp("my!", parsed))
			{
				lcompile("\tmy = tos; tos = pop; // my!\n");
				continue;
			}

			if (!strcmp("@+", parsed))
			{
				lcompile("\tpush(tos); tos = *((cell*)at); at += sizeof(cell); // @+\n");
				continue;
			}

			if (!strcmp("@-", parsed))
			{
				lcompile("\tpush(tos); tos = *((cell*)at); at -= sizeof(cell); // @-\n");
				continue;
			}

			if (!strcmp("!+", parsed))
			{
				lcompile("\t*((cell*)at) = tos; tos = pop; at += sizeof(cell); // !+\n");
				continue;
			}

			if (!strcmp("!-", parsed))
			{
				lcompile("\t*((cell*)at) = tos; tos = pop; at -= sizeof(cell); // !-\n");
				continue;
			}

			if (!strcmp("b@+", parsed))
			{
				lcompile("\tpush(tos); tos = *((unsigned char*)at); at += sizeof(unsigned char); // b@+\n");
				continue;
			}

			if (!strcmp("b@-", parsed))
			{
				lcompile("\tpush(tos); tos = *((unsigned char*)at); at -= sizeof(unsigned char); // b@-\n");
				continue;
			}

			if (!strcmp("b!+", parsed))
			{
				lcompile("\t*((unsigned char*)at) = tos; tos = pop; at += sizeof(unsigned char); // b!+\n");
				continue;
			}

			if (!strcmp("b!-", parsed))
			{
				lcompile("\t*((unsigned char*)at) = tos; tos = pop; at -= sizeof(unsigned char); // b!-\n");
				continue;
			}

			if (!strcmp("abs", parsed))
			{
				lcompile("\tif (tos < 0) tos *= -1; // abs\n");
				continue;
			}

			if (!strcmp("neg", parsed))
			{
				lcompile("\ttos *= -1; // neg\n");
				continue;
			}

			if (!strcmp("@", parsed))
			{
				lcompile("\ttos = *((cell*)tos); // @\n");
				continue;
			}
			if (!strcmp("!", parsed))
			{
				lcompile("\t*((cell*)tos) = pop; tos = pop; // !\n");
				continue;
			}
			if (!strcmp("b@", parsed))
			{
				lcompile("\ttos = *((unsigned char*)tos); // b@\n");
				continue;
			}
			if (!strcmp("b!", parsed))
			{
				lcompile("\t*((unsigned char*)tos) = pop; tos = pop; // b!\n");
				continue;
			}
			if (!strcmp("=", parsed))
			{
				lcompile("\ttos = pop == tos ? -1:0; // =\n");
				continue;
			}
			if (!strcmp("<", parsed))
			{
				lcompile("\ttos = pop < tos ? -1:0; // <\n");
				continue;
			}
			if (!strcmp(">", parsed))
			{
				lcompile("\ttos = pop > tos ? -1:0; // >\n");
				continue;
			}
			if (!strcmp("<>", parsed))
			{
				lcompile("\ttos = pop != tos ? -1:0; // <>\n");
				continue;
			}
			if (!strcmp("<=", parsed))
			{
				lcompile("\ttos = pop <= tos ? -1:0; // <=\n");
				continue;
			}
			if (!strcmp(">=", parsed))
			{
				lcompile("\ttos = pop >= tos ? -1:0; // >=\n");
				continue;
			}
			if (!strcmp("&", parsed) || !strcmp("and", parsed))
			{
				lcompile("\ttos &= pop;\n");
				continue;
			}
			if (!strcmp("^", parsed) || !strcmp("xor", parsed))
			{
				lcompile("\ttos ^= pop;\n");
				continue;
			}
			if (!strcmp("|", parsed) || !strcmp("or", parsed))
			{
				lcompile("\ttos |= pop;\n");
				continue;
			}
			if (!strcmp("~", parsed) || !strcmp("inv", parsed))
			{
				lcompile("\ttos = ~tos;\n");
				continue;
			}
			if (!strcmp("min", parsed))
			{
				lcompile("\t{ cell tmp = pop; tos = tmp < tos ? tmp:tos; } // min\n");
				continue;
			}
			if (!strcmp("max", parsed))
			{
				lcompile("\t{ cell tmp = pop; tos = tmp > tos ? tmp:tos; } // max\n");
				continue;
			}
			if (!strcmp("+", parsed) || !strcmp("add", parsed))
			{
				lcompile("\ttos += pop;\n");
				continue;
			}
			if (!strcmp("-", parsed) || !strcmp("sub", parsed))
			{
				lcompile("\ttos = pop-tos;\n");
				continue;
			}
			if (!strcmp("*", parsed) || !strcmp("mul", parsed))
			{
				lcompile("\ttos *= pop;\n");
				continue;
			}
			if (!strcmp("/", parsed) || !strcmp("div", parsed))
			{
				lcompile("\ttos = pop/tos;\n");
				continue;
			}
			if (!strcmp("mod", parsed) || !strcmp("%", parsed))
			{
				lcompile("\ttos = pop%%tos;\n");
				continue;
			}
			if (!strcmp("shl", parsed) || !strcmp("<<", parsed))
			{
				lcompile("\ttos = pop<<tos;\n");
				continue;
			}
			if (!strcmp("shr", parsed) || !strcmp(">>", parsed))
			{
				lcompile("\ttos = pop>>tos;\n");
				continue;
			}
			if (!strcmp("true", parsed))
			{
				lcompile("\tpush(tos); tos = -1; // true\n");
				continue;
			}
			if (!strcmp("false", parsed))
			{
				lcompile("\tpush(tos); tos = 0; // false\n");
				continue;
			}
			if (!strcmp("null", parsed))
			{
				lcompile("\tpush(tos); tos = 0; // null\n");
				continue;
			}
			if (!strcmp("cell+", parsed))
			{
				lcompile("\ttos += sizeof(cell);\n");
				continue;
			}
			if (!strcmp("cell-", parsed))
			{
				lcompile("\ttos -= sizeof(cell);\n");
				continue;
			}
			if (!strcmp("cell", parsed))
			{
				lcompile("\tpush(tos); tos = sizeof(cell);\n");
				continue;
			}
			if (!strcmp("cells", parsed))
			{
				lcompile("\ttos *= sizeof(cell);\n");
				continue;
			}
			if (!strcmp("byte-", parsed))
			{
				lcompile("\ttos--;\n");
				continue;
			}
			if (!strcmp("byte+", parsed))
			{
				lcompile("\ttos++;\n");
				continue;
			}
			if (!strcmp("byte", parsed))
			{
				lcompile("\tpush(tos); tos = 1;\n");
				continue;
			}
			if (!strcmp("bytes", parsed))
			{
				continue;
			}
			if (!strcmp("dup", parsed))
			{
				lcompile("\tpush(tos); // dup\n");
				continue;
			}
			if (!strcmp("drop", parsed))
			{
				lcompile("\ttos = pop; // drop\n");
				continue;
			}
			if (!strcmp("over", parsed))
			{
				lcompile("\tpush(tos); tos = sp[-2]; // over\n");
				continue;
			}
			if (!strcmp("swap", parsed))
			{
				lcompile("\t{ cell tmp = tos; tos = sp[-1]; sp[-1] = tmp; } // swap\n");
				continue;
			}
			if (!strcmp("nip", parsed))
			{
				lcompile("\tsp--; // nip\n");
				continue;
			}
			if (!strcmp("tuck", parsed))
			{
				lcompile("\t{ cell tmp = pop; push(tos); push(tmp); } // tuck\n");
				continue;
			}
			if (!strcmp("pick", parsed))
			{
				lcompile("\ttos = sp[(tos+1)*-1]; // pick\n");
				continue;
			}
			if (!strcmp("rot", parsed))
			{
				lcompile("\t{ cell tmp = tos; tos = sp[-2]; sp[-2] = sp[-1]; sp[-1] = tmp; } // rot\n");
				continue;
			}
			if (!strcmp("depth", parsed))
			{
				lcompile("\tpush(tos); tos = sp-stack-1; // depth\n");
				continue;
			}
			if (!strcmp("push", parsed))
			{
				lcompile("\t*asp++ = tos; tos = pop; // push\n");
				continue;
			}
			if (!strcmp("pop", parsed))
			{
				lcompile("\tpush(tos); tos = *--asp; // pop\n");
				continue;
			}
			if (!strcmp("top", parsed))
			{
				lcompile("\tpush(tos); tos = asp[-1]; // top\n");
				continue;
			}

			if (!strcmp("emit", parsed))
			{
				lcompile("\t*sp = tos; write(fileno(stdout), (char*)sp, 1); tos = pop;\n");
				continue;
			}

			if (!strcmp("type", parsed))
			{
				lcompile("\twrite(fileno(stdout), (char*)tos, strlen((char*)tos)); tos = pop;\n");
				continue;
			}

			if (!strcmp("error", parsed))
			{
				lcompile("\twrite(fileno(stderr), (char*)tos, strlen((char*)tos)); tos = pop;\n");
				continue;
			}

			if (!strcmp("at-xy", parsed))
			{
				lcompile("\ttos = f_at_xy(tos);\n");
				f_at_xy = 1;
				continue;
			}

			if (!strcmp("max-xy", parsed))
			{
				lcompile("\ttos = f_max_xy(tos);\n");
				f_max_xy = 1;
				continue;
			}

			if (!strcmp("key?", parsed))
			{
				lcompile("\ttos = f_keyq(tos);\n");
				f_keyq = 1;
				continue;
			}

			if (!strcmp("microsleep", parsed))
			{
				lcompile("\tusleep(tos); tos = pop; // usleep\n");
				continue;
			}

			if (!strcmp("microtime", parsed))
			{
				lcompile("\t{ struct timeval tv; gettimeofday(&tv, NULL);");
				lcompile(" push(tos); tos = 1000000 * tv.sec + tv.usec; } // utime\n");
				continue;
			}

			if (!strcmp("arg", parsed))
			{
				lcompile("\ttos = tos >= 0 && tos < argc ? (cell)argv[tos]: 0; // arg\n");
				continue;
			}

			if (!strcmp("count", parsed))
			{
				lcompile("\ttos = strlen((char*)tos); // count\n");
				continue;
			}

			if (!strcmp("execute", parsed))
			{
				lcompile("\t{ cell tmp = tos; tos = pop; if (tmp) tos = ((word)tmp)(tos); } // execute\n");
				continue;
			}

			if (!strcmp("place", parsed))
			{
				lcompile("\t{ char *dst = (char*)tos, *src = (char*)pop; memmove(dst, src, strlen(src)+1); tos = pop; } // place\n");
				continue;
			}

			if (!strcmp("move", parsed))
			{
				lcompile("\t{ char *dst = (char*)pop, *src = (char*)pop; memmove(dst, src, tos); tos = pop; } // move\n");
				continue;
			}

			if (!strcmp("allocate", parsed))
			{
				lcompile("\ttos = (cell)calloc(1, tos); // allocate\n");
				continue;
			}

			if (!strcmp("resize", parsed))
			{
				lcompile("\ttos = (cell)realloc((void*)pop, tos); // resize\n");
				continue;
			}

			if (!strcmp("free", parsed))
			{
				lcompile("\tfree((void*)tos); tos = pop; // free\n");
				continue;
			}

			if (!strcmp("getenv", parsed))
			{
				lcompile("\ttos = (cell)getenv((char*)tos); // getenv\n");
				continue;
			}

			if (!strcmp("format", parsed))
			{
				lcompile("\ttos = f_format(tos); // format\n");
				f_format = 1;
				continue;
			}

			if (!strcmp("match", parsed))
			{
				lcompile("\ttos = f_match(tos); // match\n");
				f_match = 1;
				continue;
			}

			if (!strcmp("split", parsed))
			{
				lcompile("\ttos = f_split(tos); // split\n");
				f_split = 1;
				continue;
			}

			if (!strcmp("key", parsed))
			{
				lcompile("\ttos = f_key(tos); // key\n");
				f_key = 1;
				continue;
			}

			if (!strcmp("compare", parsed))
			{
				lcompile("\ttos = f_compare(tos); // compare\n");
				f_compare = 1;
				continue;
			}

			if (!strcmp("slurp", parsed))
			{
				lcompile("\ttos = f_slurp(tos); // slurp\n");
				f_slurp = 1;
				continue;
			}

			if (!strcmp("blurt", parsed))
			{
				lcompile("\ttos = f_blurt(tos); // blurt\n");
				f_blurt = 1;
				continue;
			}

			if (!strcmp("number", parsed))
			{
				lcompile("\ttos = f_number(tos); // number\n");
				f_number = 1;
				continue;
			}

			if (!strcmp("fork", parsed))
			{
				lcompile("\ttos = f_fork(tos); // fork\n");
				f_fork = 1;
				continue;
			}

			if (!strcmp("bye", parsed))
			{
				lcompile("\ttos = f_bye(tos); // bye\n");
				continue;
			}

			if (number(parsed, &n))
			{
				lcompile("\tpush(tos); tos = %ld;\n", n);
				continue;
			}

			if (*parsed == '\'' && (n = find_name(parsed+1)))
			{
				lcompile("\tpush(tos); tos = (cell)(&f_word%d);\n", n);
				last = n;
				continue;
			}

			if (*parsed == '`' && parsed[1])
			{
				unsigned char c = parsed[1];
				lcompile("\tpush(tos); tos = %d;\n", c);
				continue;
			}

			if (match("^[0-9]+[0-9a-fA-Fx]*[-+*/%=<>]+$", parsed))
			{
				int len = strlen(parsed);
				char op = parsed[len-1];
				char tmp[1024]; strcpy(tmp, parsed);
				tmp[len-1] = 0;

				if (number(tmp, &n))
				{
					switch (op)
					{
						case '+':
							lcompile("\ttos += %ld; // %s\n", n, parsed);
							break;
						case '-':
							lcompile("\ttos -= %ld; // %s\n", n, parsed);
							break;
						case '*':
							lcompile("\ttos *= %ld; // %s\n", n, parsed);
							break;
						case '/':
							lcompile("\ttos /= %ld; // %s\n", n, parsed);
							break;
						case '%':
							lcompile("\ttos %= %ld; // %s\n", n, parsed);
							break;
						case '=':
							lcompile("\ttos = tos == %ld ?-1:0; // %s\n", n, parsed);
							break;
						case '<':
							lcompile("\ttos = tos < %ld ?-1:0; // %s\n", n, parsed);
							break;
						case '>':
							lcompile("\ttos = tos > %ld ?-1:0; // %s\n", n, parsed);
							break;
					}
					continue;
				}
			}

			if (match("^[0-9]+[0-9a-fA-Fx]*(<>|>=|<=)$", parsed))
			{
				int len = strlen(parsed);
				char tmp[1024]; strcpy(tmp, parsed);
				tmp[len-2] = 0;

				if (number(tmp, &n))
				{
					if (!strcmp(parsed + len - 2, "<>"))
					{
						lcompile("\ttos = tos != %ld ?-1:0; // %s\n", n, parsed);
					}
					else
					if (!strcmp(parsed + len - 2, "<="))
					{
						lcompile("\ttos = tos <= %ld ?-1:0; // %s\n", n, parsed);
					}
					else
					if (!strcmp(parsed + len - 2, ">="))
					{
						lcompile("\ttos = tos >= %ld ?-1:0; // %s\n", n, parsed);
					}
					continue;
				}
			}

			errx(EXIT_FAILURE, "what? %s", parsed);
		}

		free(slurped);
	}

	lcompile("\t} while(0);\n\treturn tos;\n}");
	gcompile("\t_f_main(0);");

	close_scope();

	char line[1024];
	FILE *out = fopen("build.c", "r");
	while (fgets(line, 1024, out) && !match("^// \\*\\*\\*FORTH\\*\\*\\*", line))
	{
		printf("%s", line);
	}

	if (f_format)  printf("#define F_FORMAT  1\n");
	if (f_at_xy)   printf("#define F_AT_XY   1\n");
	if (f_max_xy)  printf("#define F_MAX_XY  1\n");
	if (f_match)   printf("#define F_MATCH   1\n");
	if (f_split)   printf("#define F_SPLIT   1\n");
	if (f_key)     printf("#define F_KEY     1\n");
	if (f_keyq)    printf("#define F_KEYQ    1\n");
	if (f_compare) printf("#define F_COMPARE 1\n");
	if (f_slurp)   printf("#define F_SLURP   1\n");
	if (f_blurt)   printf("#define F_BLURT   1\n");
	if (f_number)  printf("#define F_NUMBER  1\n");
	if (f_fork)    printf("#define F_FORK    1\n");

	printf("%s\n\n", all_proto);
	printf("%s\n\n", all_local);

	while (fgets(line, 1024, out) && !match("^// \\*\\*\\*MAIN\\*\\*\\*", line))
	{
		printf("%s", line);
	}

	printf("%s", all_global);

	while (fgets(line, 1024, out))
	{
		printf("%s", line);
	}

	return 0;
}
