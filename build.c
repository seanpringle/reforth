// asdadad

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <regex.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef int64_t cell;
typedef cell (*word)(cell);

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


// ***FORTH***

int
main (int argc, char *argv[])
{
	sp  = stack;
	asp = astack;

// ***MAIN***

	return 0;
}
