
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
	//fputc(c, stdout);
}

char scanc(char *s)
{
	char c;
	while ((c = fgetc(in)) && c != EOF && !strchr(s, c));
	return c;
}

int main(int argc, char *argv[])
{
	int arg, i, j, len;
	char c, *ptr, name[32], file[32], *data;

	for (arg = 1; arg < argc; arg++)
	{
		if (!strcmp(argv[arg], "--turnkey") && arg+1 < argc)
		{
			arg++;
			printf("%s\n", argv[arg]);
			len = sprintf(name, "src_turnkey");
		}
		else
		{
			printf("%s\n", argv[arg]);

			ptr = argv[arg];
			len = sprintf(name, "src_");
			while (*ptr && isalnum(*ptr))
				name[len++] = *ptr++;
			name[len] = 0;
		}

		sprintf(file, "%s.c", name);

		in = fopen(argv[arg], "r");
		sanity(in, "could not open %s", argv[arg]);

		out = fopen(file, "w+");
		sanity(out, "could not create %s", file);

		fprintf(out, "const char %s[] = {\n", name);

		data = calloc(2, 1); len = 0;
		while ((c = fgetc(in)) && c != EOF)
		{
			data = realloc(data, len + 2);
			data[len++] = c;
		}
		data[len] = 0;
		fclose(in);

		for (i = 0, j = 0; i < len;)
		{
			c = data[i];

			if (isspace(c))
			{
				while (i < len && (c = data[i]) && isspace(c)) i++;
			}

			c = data[i];

			if (c == '"')
			{
				outc(' ', j++);
				outc(c, j++); i++;
				while (i < len && (c = data[i]) && c)
				{
					outc(c, j++);
					i++;

					if (c == '"')
						break;

					if (c == '\\' && data[i])
					{
						outc(data[i], j++);
						i++;
					}
				}
				continue;
			}

			if (c == '\\' && isspace(data[i+1]))
			{
				while (i < len && (c = data[i]) && c && c != '\n') i++;
				if (c == '\n') i++;
				continue;
			}

			if (c == '(' && isspace(data[i+1]))
			{
				while (i < len && (c = data[i]) && c && c != ')') i++;
				if (c == ')') i++;
				continue;
			}

			outc(' ', j++);
			while (i < len && (c = data[i]) && c && !isspace(c))
			{
				outc(c, j++);
				i++;
			}
		}

		fprintf(out, " 0 };");
		fclose(out);
	}
	return EXIT_SUCCESS;
}
