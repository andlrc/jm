#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include "jx.h"

static inline void print_version(void)
{
	printf("%s: %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

static inline void print_usage(void)
{
	fprintf(stderr, "Usage: %s [OPTION] <file>\n", PROGRAM_NAME);
}

static inline void print_help(void)
{
	printf("Usage: %s [OPTION] <file>\n\n"
	       "  -o           Output suffix, if left out then stdout is used\n"
	       "  -p           Prettify the output json\n"
	       "  -v           Set key=value variable\n"
	       "  -h           Show this help and exit\n", PROGRAM_NAME);
}

static struct option const long_options[] = {
	{"version", no_argument, NULL, 'V'},
	{"help", no_argument, NULL, 'h'},
	{"suffix", required_argument, NULL, 's'},
	{"pretty", no_argument, NULL, 'p'},
	{"variable", required_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

char short_options[] = "Vhs:pv:";

int main(int argc, char **argv)
{
	jx_object_t *vars = NULL;
	char *suffix = NULL, *tmpbuf, *varkey, *infile;
	size_t suflen = 0;
	int ch, argind, pretty = 0;

	if ((vars = jx_newObject()) == NULL)
		exit(EXIT_FAILURE);

	while ((ch =
		getopt_long(argc, argv, short_options, long_options,
			    NULL)) != -1) {
		switch (ch) {
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
			break;
		case 's':
			suffix = optarg;
			suflen = strlen(suffix);
			break;
		case 'p':
			pretty = 1;
			break;
		case 'v':
			tmpbuf = strdup(optarg);
			varkey = tmpbuf;
			while (*tmpbuf != '\0' && *tmpbuf != '=') {
				tmpbuf++;
			}
			*tmpbuf++ = '\0';
			jx_moveInto(vars, varkey, jx_newString(tmpbuf));
			free(varkey);
			break;
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	argind = optind;
	infile = "-";

	do {
		jx_object_t *root = NULL, *out = NULL;

		if (argind < argc)
			infile = argv[argind];

		if ((root = jx_parseFile(infile)) == NULL)
			continue;


		out = jx_merge(root, vars);
		if (out == NULL) {
			jx_free(root);
			continue;
		}
		jx_free(root);

		if (strcmp(infile, "-") != 0 && suffix) {
			char *outfile = NULL;
			size_t inlen = strlen(infile);
			if ((outfile = malloc(inlen + suflen + 1)) == NULL)
				continue;
			memcpy(outfile, infile, inlen);
			memcpy(outfile + inlen, suffix, suflen + 1);
			jx_serialize(outfile, out, pretty ? JX_PRETTY : 0);
			free(outfile);
		} else {
			jx_serialize("-", out, pretty ? JX_PRETTY : 0);
		}

		jx_free(out);
	} while (++argind < argc);

	jx_free(vars);

	exit(EXIT_SUCCESS);
}
