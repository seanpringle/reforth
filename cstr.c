
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

int main(int argc, char *argv[])
{
	char c;
	int i;

	if (argc < 3) exit(EXIT_FAILURE);
	printf("const char src_%s[] = {", argv[1]);
	FILE *f = fopen(argv[2], "r");
	if (!f) exit(EXIT_FAILURE);
	while ((c = fgetc(f)) && c != EOF)
		printf("%d,", (int)c);
	printf("0};");
	fclose(f);
	return EXIT_SUCCESS;
}
