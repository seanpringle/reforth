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
	cell n;
	all_proto  = calloc(1, 1024);
	all_local  = calloc(1, 1024);
	all_global = calloc(1, 1024);

	open_scope("_global_");
	lcompile("cell _f_main(cell tos) {\n");
	lcompile("\tcell at, my, tmp;\n");

	int _stk[1024], *stk = _stk;

	src = slurp(argv[1]);

	while (*src)
	{
		while (isspace(*src)) src++;

		if (*src == '"')
		{
			n = 0; char c;
			parsed[n++] = *src++;
			while ((c = *src++))
			{
				parsed[n++] = c;
				if (c == '"')
					break;
				if (c == '\\' && *src)
					parsed[n++] = *src++;
			}
			parsed[n] = 0;
			lcompile("\tpush(tos); tos = (cell)%s;\n", parsed);
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

		if (!strcmp(":", parsed))
		{
			assert(parse());
			open_scope(parsed);
			pcompile("static cell f_word%d(cell);\n", word);
			lcompile("static cell f_word%d(cell tos) { //%s\n", word, parsed);
			lcompile("\tcell at, my, tmp;\n");
			*stk++ = word;
			continue;
		}

		if (!strcmp("field", parsed))
		{
			assert(parse());
			open_scope(parsed);
			pcompile("static cell f_word%d(cell);\n", word);
			lcompile("static cell f_word%d(cell tos) { return tos + (%d * sizeof(cell)); }\n", word, vars[stk[-1]]);
			vars[stk[-1]]++;
			close_scope();
			continue;
		}

		if (!strcmp("make", parsed))
		{
			lcompile("\tpush(tos); tos = (cell)calloc(f_vars%d, sizeof(cell));\n", stk[-1]);
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
			pcompile("cell f_val%d; //%s\n", word, parsed);
			pcompile("cell f_word%d(cell tos) { push(tos); return f_val%d; }\n", word, word);
			close_scope();
			lcompile("\tf_val%d = tos; tos = pop; //%s\n", word, parsed);
			continue;
		}

		if (!strcmp("to", parsed))
		{
			assert(parse());
			int id = find_name(parsed);
			assert(id);
			lcompile("\tf_val%d = tos; tos = pop;\n", id);
			continue;
		}

		if (!strcmp("if", parsed))
		{
			lcompile("\tdo { tmp = tos; tos = pop; if (tmp) {\n");
			continue;
		}

		if (!strcmp("else", parsed))
		{
			lcompile("\t} else {\n");
			continue;
		}

		if (!strcmp("for", parsed))
		{
			lcompile("\tdo { int i, l = tos; tos = pop; for (i = 0; i < l; i++) {\n");
			continue;
		}

		if (!strcmp("begin", parsed))
		{
			lcompile("\tdo { int i; for (i = 0; ; i++) {\n");
			continue;
		}

		if (!strcmp("end", parsed))
		{
			lcompile("\t} } while(0);\n");
			continue;
		}

		if (!strcmp("i", parsed))
		{
			lcompile("\tpush(tos); tos = i;\n");
			continue;
		}

		if (!strcmp("while", parsed))
		{
			lcompile("\ttmp = tos; tos = pop; if (!tmp) break;\n");
			continue;
		}

		if (!strcmp("until", parsed))
		{
			lcompile("\ttmp = tos; tos = pop; if (tmp) break;\n");
			continue;
		}

		if (!strcmp("leave", parsed))
		{
			lcompile("\tbreak;\n");
			continue;
		}

		if (!strcmp("next", parsed))
		{
			lcompile("\tcontinue;\n");
			continue;
		}

		if (!strcmp("exit", parsed))
		{
			lcompile("\treturn tos;\n");
			continue;
		}

		if (!strcmp("at", parsed))
		{
			lcompile("\tpush(tos); tos = at;\n");
			continue;
		}

		if (!strcmp("my", parsed))
		{
			lcompile("\tpush(tos); tos = my;\n");
			continue;
		}

		if (!strcmp("at!", parsed))
		{
			lcompile("\tat = tos; tos = pop;\n");
			continue;
		}

		if (!strcmp("my!", parsed))
		{
			lcompile("\tmy = tos; tos = pop;\n");
			continue;
		}

		if (!strcmp("@+", parsed))
		{
			lcompile("\tpush(tos); tos = *((cell*)at); at += sizeof(cell);\n");
			continue;
		}

		if (!strcmp("!+", parsed))
		{
			lcompile("\t*((cell*)at) = tos; tos = pop; at += sizeof(cell);\n");
			continue;
		}

		if (!strcmp("c@+", parsed))
		{
			lcompile("\tpush(tos); tos = *((unsigned char*)at); at += sizeof(unsigned char);\n");
			continue;
		}

		if (!strcmp("c!+", parsed))
		{
			lcompile("\t*((unsigned char*)at) = tos; tos = pop; at += sizeof(unsigned char);\n");
			continue;
		}

		if (!strcmp("1+", parsed))
		{
			lcompile("\ttos++;\n");
			continue;
		}

		if (!strcmp("1-", parsed))
		{
			lcompile("\ttos--;\n");
			continue;
		}

		if (!strcmp("2*", parsed))
		{
			lcompile("\ttos<<1;\n");
			continue;
		}

		if (!strcmp("2/", parsed))
		{
			lcompile("\ttos>>1;\n");
			continue;
		}

		if (!strcmp("abs", parsed))
		{
			lcompile("\tif (tos < 0) tos *= -1;\n");
			continue;
		}

		if ((n = find_name(parsed)))
		{
			lcompile("\ttos = f_word%d(tos); //%s\n", n, parsed);
			continue;
		}

		if (!strcmp("@", parsed))
		{
			lcompile("\ttos = *((cell*)tos);\n");
			continue;
		}
		if (!strcmp("!", parsed))
		{
			lcompile("\t*((cell*)tos) = pop; tos = pop;\n");
			continue;
		}
		if (!strcmp("c@", parsed))
		{
			lcompile("\ttos = *((unsigned char*)tos);\n");
			continue;
		}
		if (!strcmp("c!", parsed))
		{
			lcompile("\t*((unsigned char*)tos) = pop; tos = pop;\n");
			continue;
		}
		if (!strcmp("=", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp == tos ? -1:0;\n");
			continue;
		}
		if (!strcmp("<>", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp != tos ? -1:0;\n");
			continue;
		}
		if (!strcmp("<", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp < tos ? -1:0;\n");
			continue;
		}
		if (!strcmp(">", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp > tos ? -1:0;\n");
			continue;
		}
		if (!strcmp("<=", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp <= tos ? -1:0;\n");
			continue;
		}
		if (!strcmp(">=", parsed))
		{
			lcompile("\ttmp = pop; tos = tmp >= tos ? -1:0;\n");
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
		if (!strcmp("cells", parsed))
		{
			lcompile("\ttos *= sizeof(cell);\n");
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
			lcompile("\twrite(fileno(stdout), &tos, 1); tos = pop;\n");
			continue;
		}

		if (!strcmp("type", parsed))
		{
			lcompile("\twrite(fileno(stdout), (char*)tos, strlen((char*)tos)); tos = pop;\n");
			continue;
		}

		if (
			match("^(emit|type|allocate|resize|free|count|place|cmove|move)$", parsed) ||
			match("^(format|getenv)$", parsed)
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

		errx(EXIT_FAILURE, "what? %s", parsed);
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