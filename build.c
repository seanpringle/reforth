// asdadad

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <regex.h>
#include <assert.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef int64_t cell;
typedef cell (*word)(cell);

int argc; char **argv;
cell stack[64], *sp, astack[64], *asp;

#define push(n) (*sp++ = (cell)(n))
#define pop (*--sp)

static cell
f_dot(cell tos)
{
	char tmp[10];
	snprintf(tmp, 10, "%ld ", tos);
	write(fileno(stdout), tmp, strlen(tmp));
	return pop;
}

static cell f_count(cell tos) { return strlen((char*)tos); }
static cell f_place(cell tos) { char *dst = (char*)tos, *src = (char*)pop; memmove(dst, src, strlen(src)+1); return pop; }
static cell f_cmove(cell tos) { char *dst = (char*)pop, *src = (char*)pop; memmove(dst, src, tos); return pop; }
static cell f_move(cell tos)  { char *dst = (char*)pop, *src = (char*)pop; memmove(dst, src, tos * sizeof(cell)); return pop; }

static cell f_allocate(cell tos) { return (cell)calloc(1, tos); }
static cell f_resize(cell tos) { return (cell)realloc((void*)pop, tos); }
static cell f_free(cell tos) { free((void*)tos); return pop; }

static cell f_getenv(cell tos) { return (cell)getenv((char*)tos); }

static int format_i;
#define FORMAT_BUF 1024*64
#define FORMAT_BUFS 3
static char *format_bufs[FORMAT_BUFS];

// Format a string using C-like printf() syntax
static cell
f_format(cell tos)
{
	char *in = (char*)tos;
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
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, (char*)pop);
				}
				else
				if (strchr("cdiouxX", c))
				{
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, pop);
				}
				else
				if (strchr("eEfgG", c))
				{
					len += snprintf(buf+len, FORMAT_BUF-len, tmp, (double)pop);
				}
				continue;
			}
		}
		buf[len++] = c;
	}
	buf[len] = 0;
	free(format_bufs[idx]);
	format_bufs[idx] = buf;
	return (cell)buf;
}

static cell
f_at_xy(cell tos)
{
	cell x = pop;
	cell y = tos;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	x++; y++;

	char tmp[32];
	sprintf(tmp, "\e[%d;%dH", (int)y, (int)x);
	write(fileno(stdout), tmp, strlen(tmp));

	return pop;
}

static cell
f_max_xy(cell tos)
{
	push(tos);
	struct winsize ws;
	ioctl(0, TIOCGWINSZ, &ws);
	push(ws.ws_col-1);
	return ws.ws_row-1;
}

static cell
f_key(cell tos)
{
	push(tos);
	char c;
	read(STDIN_FILENO, &c, 1);
	return c;
}

static cell
f_keyq(cell tos)
{
	push(tos);
	struct pollfd fds;
	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;
	fds.revents = 0;
	return poll(&fds, 1, 0) > 0 ? -1:0;
}

#define REGEX_CACHE 4
static char *re_patterns[REGEX_CACHE];
static regex_t re_compiled[REGEX_CACHE];

static regex_t*
regex(char *pattern)
{
	regex_t *re = NULL; int i;

	// have we compiled this regex before?
	for (i = 0; i < REGEX_CACHE; i++)
	{
		if (re_patterns[i] && !strcmp(pattern, re_patterns[i]))
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
			fprintf(stderr, "regex compile failure: %s", pattern);
			exit(1);
		}
	}
	return re;
}

// POSIX regex match
static cell
f_match(cell tos)
{
	char *pattern = (char*)tos;
	char *subject = (char*)pop;
	int r = 0; regmatch_t pmatch;
	regex_t *re = regex(pattern);
	if (subject)
	{
		r = regexec(re, subject, 1, &pmatch, 0) == 0 ?-1:0;
		subject += r ? pmatch.rm_so: strlen(subject);
	}
	push(subject);
	return (cell)r;
}

// POSIX regex split
static cell
f_split(cell tos)
{
	char *pattern = (char*)tos;
	char *subject = (char*)pop;
	int r = 0; regmatch_t pmatch;
	regex_t *re = regex(pattern);
	if (subject)
	{
		r = regexec(re, subject, 1, &pmatch, 0) == 0 ?-1:0;
		if (r)
		{
			memset(subject + pmatch.rm_so, 0, pmatch.rm_eo - pmatch.rm_so);
			subject += pmatch.rm_eo;
		}
		else
		{
			subject += strlen(subject);
		}
	}
	push(subject);
	return (cell)r;
}

static cell
f_compare(cell tos)
{
	cell tmp = tos;
	char* s2 = (char*)pop;
	char* s1 = (char*)pop;
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
	return tos;
}

// Read a file into memory
static cell
f_slurp(cell tos)
{
	char *name = (char*)tos;
	int len = 0, lim = 1024;
	char *pad = malloc(lim+1);

	FILE *file = fopen(name, "r");
	if (!file) return 0;

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
	return (cell)pad;
}

// Write a file to disk
static cell
f_blurt(cell tos)
{
	const char *name = (char*)tos;
	const char *data = (char*)pop;
	unsigned int dlen = strlen(data);
	FILE *f = fopen(name, "w+");
	if (!f || fwrite(data, 1, dlen, f) != dlen) return 0;
	fclose(f);
	return 1;
}

static cell
f_execute(cell tos)
{
	word w = (word)tos;
	tos = pop;
	if (w) tos = w(tos);
	return tos;
}

// Convert a string to a number
cell
f_number(cell tos)
{
	char *conv = (char*)tos;
	char str[32];

	if (!conv || !conv[0])
	{
		push(0);
		return 0;
	}

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
		push(strtol(str, &err, base));
		return (err == str + strlen(str)) ? -1: 0;
	}
	push(0);
	return 0;
}

struct termios old_tio, new_tio;

cell
f_bye(cell tos)
{
	if (isatty(STDIN_FILENO))
		tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
	exit(0);
	return tos;
}

void handler(int sig)
{
	if (sig == SIGTERM || sig == SIGQUIT || (isatty(STDIN_FILENO) && sig == SIGHUP))
	{
		f_bye(0);
	}
}

// ***FORTH***

int
main (int _argc, char *_argv[])
{
	argc = _argc;
	argv = _argv;

	signal(SIGTERM, handler);
	signal(SIGQUIT, handler);
	signal(SIGHUP,  handler);
	signal(SIGINT,  handler);

	if (isatty(STDIN_FILENO))
	{
		tcgetattr(STDIN_FILENO,&old_tio);

		new_tio = old_tio;
		new_tio.c_lflag &= (~ICANON & ~ECHO);
		new_tio.c_cc[VTIME] = 0;
		new_tio.c_cc[VMIN] = 1;
		tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
	}

	sp  = stack;
	asp = astack;

// ***MAIN***

	f_bye(0);
	return 0;
}
