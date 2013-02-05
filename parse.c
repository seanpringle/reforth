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
	//fprintf(stderr, "parse: %s\n", parsed);
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
int vars[65535];
int word, cword;

char *sources[1024];
int source;

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

	int i;
	for (i = strlen(namespace); i >= 0; i--)
		if (namespace[i] == ':')
			break;
	namespace[i] = 0;
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
//		fprintf(stderr, "looking for: %s\n", alias);
		int i;
		for (i = word; i > 0; i--)
		{
//			fprintf(stderr, "    trying: %s\n", words[i]);
			if (!strcmp(words[i], alias))
				return i;
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

	open_scope("_global_");
	lcompile("cell _f_main(cell tos) {\n");
	lcompile("\tcell at, my, tmp;\n");

	int _stk[1024], *stk = _stk;

	for (arg = 1; arg < argc; arg++)
	{
		char *slurped = slurp(argv[1]);
		src = slurped;

		while (*src)
		{
			while (isspace(*src)) src++;

			if (*src == '"')
			{
				sparse();
				pcompile("const char string%d[] = {", strings);
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
				pcompile("static cell f_word%d(cell); //%s\n", word, parsed);
				lcompile("static cell f_word%d(cell tos) { //%s\n", word, parsed);
				lcompile("\tregister cell at, my; cell tmp;\n");
				*stk++ = word;
				continue;
			}

			if (!strcmp("field", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_word%d(cell); //%s\n", word, parsed);
				lcompile("static cell f_word%d(cell tos) { return tos + (%d * sizeof(cell)); } //%s\n", word, vars[stk[-1]], parsed);
				vars[stk[-1]]++;
				close_scope();
				continue;
			}

			if (!strcmp("make", parsed))
			{
				lcompile("\tpush(tos); tos = (cell)calloc(f_vars%d, sizeof(cell)); //make\n", stk[-1]);
				continue;
			}

			if (!strcmp(";", parsed))
			{
				lcompile("\treturn tos;\n}\n");
				pcompile("#define f_vars%d %d\n", stk[-1], vars[stk[-1]]);
				close_scope();
				stk--;
				continue;
			}

			if (!strcmp("value", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_val%d; //%s\n", word, parsed);
				pcompile("static cell f_word%d(cell tos) { push(tos); return f_val%d; } //value\n", word, word);
				close_scope();
				lcompile("\tf_val%d = tos; tos = pop; //%s\n", word, parsed);
				continue;
			}

			if (!strcmp("as", parsed))
			{
				assert(parse());
				open_scope(parsed);
				close_scope();
				lcompile("#define f_word%d(t) push(tos) || 1 ? f_val%d: f_val%d; //local %s\n", word, word, word, parsed);
				lcompile("\tcell f_val%d = tos; tos = pop; //%s\n", word, parsed);
				continue;
			}

			if (!strcmp("bind", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_val%d; //%s\n", word, parsed);
				pcompile("static word f_bind%d; //%s\n", word, parsed);
				pcompile("static cell f_word%d(cell tos) { push(tos); return f_bind%d(f_val%d); } //with\n", word, word, word);
				close_scope();
				lcompile("\tf_bind%d = (word)tos; tos = pop; //%s\n", word, parsed);
				lcompile("\tf_val%d = tos; tos = pop; //%s\n", word, parsed);
				continue;
			}

			if (!strcmp("to", parsed))
			{
				assert(parse());
				assert((n = find_name(parsed)));
				lcompile("\tf_val%d = tos; tos = pop; //to %s\n", n, parsed);
				continue;
			}

			if (!strcmp("is", parsed))
			{
				assert(parse());
				assert((n = find_name(parsed)));
				lcompile("\tf_bind%d = (word)tos; tos = pop; //%s\n", n, parsed);
				continue;
			}

			if (!strcmp("array", parsed))
			{
				assert(parse());
				open_scope(parsed);
				pcompile("static cell f_size%d, f_cell%d; //array %s\n", word, word, parsed);
				pcompile("static char *f_addr%d; //array %s\n", word, parsed);
				pcompile("static cell f_word%d(cell tos) { //array %s\n", word, parsed);
				pcompile("\tassert(tos >= 0 && tos < f_size%d);\n", word);
				pcompile("\treturn (cell)(f_addr%d + (tos * f_cell%d));\n", word, word);
				pcompile("}\n");
				close_scope();
				lcompile("\nf_cell%d = tos; tos = pop;\n", word);
				lcompile("\nf_size%d = tos; tos = pop;\n", word);
				lcompile("\tfree(f_addr%d); f_addr%d = calloc(f_size%d, f_cell%d); //%s\n", word, word, word, word, parsed);
				continue;
			}

			if (!strcmp("if", parsed))
			{
				lcompile("\t{ tmp = tos; tos = pop; if (tmp) { //if\n");
				continue;
			}

			if (!strcmp("else", parsed))
			{
				lcompile("\t} else { //else\n");
				continue;
			}

			if (!strcmp("for", parsed))
			{
				lcompile("\t{ int i, l = tos; tos = pop; for (i = 0; i < l; i++) { //for\n");
				continue;
			}

			if (!strcmp("begin", parsed))
			{
				lcompile("\t{ int i; for (i = 0; ; i++) { //begin\n");
				continue;
			}

			if (!strcmp("end", parsed))
			{
				lcompile("\t}} //end\n");
				continue;
			}

			if (!strcmp("i", parsed))
			{
				lcompile("\tpush(tos); tos = i; //i\n");
				continue;
			}

			if (!strcmp("while", parsed))
			{
				lcompile("\ttmp = tos; tos = pop; if (!tmp) break; //while\n");
				continue;
			}

			if (!strcmp("until", parsed))
			{
				lcompile("\ttmp = tos; tos = pop; if (tmp) break; //until\n");
				continue;
			}

			if (!strcmp("leave", parsed))
			{
				lcompile("\tbreak; //leave\n");
				continue;
			}

			if (!strcmp("next", parsed))
			{
				lcompile("\tcontinue; //next\n");
				continue;
			}

			if (!strcmp("exit", parsed))
			{
				lcompile("\treturn tos; //exit\n");
				continue;
			}

			if (!strcmp("at", parsed))
			{
				lcompile("\tpush(tos); tos = at; //at\n");
				continue;
			}

			if (!strcmp("my", parsed))
			{
				lcompile("\tpush(tos); tos = my; //my\n");
				continue;
			}

			if (!strcmp("at!", parsed))
			{
				lcompile("\tat = tos; tos = pop; //at!\n");
				continue;
			}

			if (!strcmp("my!", parsed))
			{
				lcompile("\tmy = tos; tos = pop; //my!\n");
				continue;
			}

			if (!strcmp("@+", parsed))
			{
				lcompile("\tpush(tos); tos = *((cell*)at); at += sizeof(cell); //@+\n");
				continue;
			}

			if (!strcmp("@-", parsed))
			{
				lcompile("\tpush(tos); tos = *((cell*)at); at -= sizeof(cell); //@-\n");
				continue;
			}

			if (!strcmp("!+", parsed))
			{
				lcompile("\t*((cell*)at) = tos; tos = pop; at += sizeof(cell); //!+\n");
				continue;
			}

			if (!strcmp("!-", parsed))
			{
				lcompile("\t*((cell*)at) = tos; tos = pop; at -= sizeof(cell); //!-\n");
				continue;
			}

			if (!strcmp("c@+", parsed))
			{
				lcompile("\tpush(tos); tos = *((unsigned char*)at); at += sizeof(unsigned char); //c@+\n");
				continue;
			}

			if (!strcmp("c@-", parsed))
			{
				lcompile("\tpush(tos); tos = *((unsigned char*)at); at -= sizeof(unsigned char); //c@-\n");
				continue;
			}

			if (!strcmp("c!+", parsed))
			{
				lcompile("\t*((unsigned char*)at) = tos; tos = pop; at += sizeof(unsigned char); //c!+\n");
				continue;
			}

			if (!strcmp("c!-", parsed))
			{
				lcompile("\t*((unsigned char*)at) = tos; tos = pop; at -= sizeof(unsigned char); //c!-\n");
				continue;
			}

			if (!strcmp("1+", parsed))
			{
				lcompile("\ttos++; //1+\n");
				continue;
			}

			if (!strcmp("1-", parsed))
			{
				lcompile("\ttos--; //1-\n");
				continue;
			}

			if (!strcmp("2*", parsed))
			{
				lcompile("\ttos = tos<<1; //2*\n");
				continue;
			}

			if (!strcmp("2/", parsed))
			{
				lcompile("\ttos = tos>>1; //2/\n");
				continue;
			}

			if (!strcmp("0=", parsed))
			{
				lcompile("\ttos = tos == 0?-1:0; //0=\n");
				continue;
			}

			if (!strcmp("0<>", parsed))
			{
				lcompile("\ttos = tos != 0?-1:0; //0<>\n");
				continue;
			}

			if (!strcmp("0<", parsed))
			{
				lcompile("\ttos = tos < 0?-1:0; //0<\n");
				continue;
			}

			if (!strcmp("0>", parsed))
			{
				lcompile("\ttos = tos > 0?-1:0; //0>\n");
				continue;
			}

			if (!strcmp("abs", parsed))
			{
				lcompile("\tif (tos < 0) tos *= -1; //abs\n");
				continue;
			}

			if (!strcmp("neg", parsed))
			{
				lcompile("\ttos *= -1; //neg\n");
				continue;
			}

			if ((n = find_name(parsed)))
			{
				lcompile("\ttos = f_word%d(tos); //%s\n", n, parsed);
				continue;
			}

			if (!strcmp("@", parsed))
			{
				lcompile("\ttos = *((cell*)tos); //@\n");
				continue;
			}
			if (!strcmp("!", parsed))
			{
				lcompile("\t*((cell*)tos) = pop; tos = pop; //!\n");
				continue;
			}
			if (!strcmp("+!", parsed))
			{
				lcompile("\t*((cell*)tos) += pop; tos = pop; //+!\n");
				continue;
			}
			if (!strcmp("c@", parsed))
			{
				lcompile("\ttos = *((unsigned char*)tos); //c@\n");
				continue;
			}
			if (!strcmp("c!", parsed))
			{
				lcompile("\t*((unsigned char*)tos) = pop; tos = pop; //c!\n");
				continue;
			}
			if (!strcmp("=", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp == tos ? -1:0; //=\n");
				continue;
			}
			if (!strcmp("<>", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp != tos ? -1:0; //<>\n");
				continue;
			}
			if (!strcmp("<", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp < tos ? -1:0; //<\n");
				continue;
			}
			if (!strcmp(">", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp > tos ? -1:0; //>\n");
				continue;
			}
			if (!strcmp("<=", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp <= tos ? -1:0; //<=\n");
				continue;
			}
			if (!strcmp(">=", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp >= tos ? -1:0; //>=\n");
				continue;
			}
			if (!strcmp("and", parsed))
			{
				lcompile("\ttos &= pop;\n");
				continue;
			}
			if (!strcmp("xor", parsed))
			{
				lcompile("\ttos ^= pop;\n");
				continue;
			}
			if (!strcmp("or", parsed))
			{
				lcompile("\ttos |= pop;\n");
				continue;
			}
			if (!strcmp("min", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp < tos ? tmp:tos;\n");
				continue;
			}
			if (!strcmp("max", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp > tos ? tmp:tos;\n");
				continue;
			}
			if (!strcmp("+", parsed))
			{
				lcompile("\ttos += pop;\n");
				continue;
			}
			if (!strcmp("-", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp-tos;\n");
				continue;
			}
			if (!strcmp("*", parsed))
			{
				lcompile("\ttos *= pop;\n");
				continue;
			}
			if (!strcmp("/", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp/tos;\n");
				continue;
			}
			if (!strcmp("mod", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp%%tos;\n");
				continue;
			}
			if (!strcmp("shl", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp<<tos;\n");
				continue;
			}
			if (!strcmp("shr", parsed))
			{
				lcompile("\ttmp = pop; tos = tmp>>tos;\n");
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
				lcompile("\tpush(tos);\n");
				continue;
			}
			if (!strcmp("drop", parsed))
			{
				lcompile("\ttos = pop;\n");
				continue;
			}
			if (!strcmp("over", parsed))
			{
				lcompile("\tpush(tos); tos = sp[-2];\n");
				continue;
			}
			if (!strcmp("swap", parsed))
			{
				lcompile("\ttmp = tos; tos = sp[-1]; sp[-1] = tmp;\n");
				continue;
			}
			if (!strcmp("nip", parsed))
			{
				lcompile("\tsp--;\n");
				continue;
			}
			if (!strcmp("tuck", parsed))
			{
				lcompile("\ttmp = pop; push(tos); push(tmp);\n");
				continue;
			}
			if (!strcmp("pick", parsed))
			{
				lcompile("\ttos = sp[(tos+1)*-1];\n");
				continue;
			}
			if (!strcmp("rot", parsed))
			{
				lcompile("\ttmp = tos; tos = sp[-2]; sp[-2] = sp[-1]; sp[-1] = tmp;\n");
				continue;
			}
			if (!strcmp("depth", parsed))
			{
				lcompile("\tpush(tos); tos = sp-stack-1;\n");
				continue;
			}
			if (!strcmp("push", parsed))
			{
				lcompile("\t*asp++ = tos; tos = pop;\n");
				continue;
			}
			if (!strcmp("pop", parsed))
			{
				lcompile("\tpush(tos); tos = *--asp;\n");
				continue;
			}
			if (!strcmp("top", parsed))
			{
				lcompile("\tpush(tos); tos = asp[-1];\n");
				continue;
			}

			if (!strcmp(".", parsed))
			{
				lcompile("\ttos = f_dot(tos);\n");
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
				continue;
			}

			if (!strcmp("max-xy", parsed))
			{
				lcompile("\ttos = f_max_xy(tos);\n");
				continue;
			}

			if (!strcmp("key?", parsed))
			{
				lcompile("\ttos = f_keyq(tos);\n");
				continue;
			}

			if (!strcmp("usec", parsed))
			{
				lcompile("\tusleep(tos); tos = pop;\n");
				continue;
			}

			if (!strcmp("arg", parsed))
			{
				lcompile("\ttos = tos >= 0 && tos < argc ? (cell)argv[tos]: 0;\n");
				continue;
			}

			if (!strcmp("count", parsed))
			{
				lcompile("\ttos = strlen((char*)tos);\n");
				continue;
			}

			if (!strcmp("execute", parsed))
			{
				lcompile("\ttmp = tos; tos = pop; if (tmp) tos = ((word)tmp)(tos);\n");
				continue;
			}

			if (
				match("^(bye|key|allocate|resize|free|place|cmove|move)$", parsed) ||
				match("^(number|slurp|blurt|compare|match|split|format|getenv)$", parsed)
			)
			{
				lcompile("\ttos = f_%s(tos);\n", parsed);
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
				continue;
			}

			if (*parsed == '`' && parsed[1])
			{
				unsigned char c = parsed[1];
				lcompile("\tpush(tos); tos = %d;\n", c);
				continue;
			}

			errx(EXIT_FAILURE, "what? %s", parsed);
		}

		free(slurped);
	}

	lcompile("\treturn tos;\n}");
	gcompile("_f_main(0);");

	close_scope();

	char line[1024];
	FILE *out = fopen("build.c", "r");
	while (fgets(line, 1024, out) && !match("^// \\*\\*\\*FORTH\\*\\*\\*", line))
	{
		printf("%s", line);
	}

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
