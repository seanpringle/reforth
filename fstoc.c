
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

#define BLOCK 1024

#define sanity(f, ...) if (!f) err(EXIT_FAILURE, __VA_ARGS__)

FILE *in, *out;

void outc(char c, int n)
{
	if (n && !(n%20)) fprintf(out, "\n");
	fprintf(out, "%3d, ", c);
}

char scanc(char *s)
{
	char c;
	while ((c = fgetc(in)) && c != EOF && !strchr(s, c));
	return c;
}

int main(int argc, char *argv[])
{
	int i, j, len, lit;
	char c, d, *ptr, name[32], file[32];

	for (i = 1; i < argc; i++)
	{
		printf("%s\n", argv[i]);

		ptr = argv[i];
		len = sprintf(name, "src_");
		while (*ptr && isalnum(*ptr))
			name[len++] = *ptr++;
		name[len] = 0;

		sprintf(file, "%s.c", name);

		in = fopen(argv[i], "r");
		sanity(in, "could not open %s", argv[i]);

		out = fopen(file, "w+");
		sanity(out, "could not create %s", file);

		fprintf(out, "const char %s[] = {\n", name);
		for (j = 0, d = 0, lit = 0; (c = fgetc(in)) && c != EOF;)
		{
			// skip commented lines
			if (!lit && c == '\\') // && (d == '\n' || !d))
			{
				scanc("\n");
				continue;
			}

			// skip bracket comments
			if (!lit && c == '(')
			{
				scanc(")");
				continue;
			}

			// compress white space
			if (!lit && isspace(c) && (isspace(d) || !d))
				continue;

			d = c;

			// conversions
			if (!lit && isspace(c))
				c = ' ';

			// detect literal strings
			if (!lit && c == '"') lit = 1;
			else if (lit  && c == '"') lit = 0;

			outc(c, j); j++;
		}
		fprintf(out, " 0 };");

		fclose(in);
		fclose(out);
	}
	return EXIT_SUCCESS;
}
