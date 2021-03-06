#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include "jm.h"

static void print_version(void)
{
	printf("%s: %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

static void print_usage(void)
{
	fprintf(stderr, "Usage: %s [OPTION] <file>\n", PROGRAM_NAME);
}

static void print_help(void)
{
	printf("Usage: %s [OPTION] <file>\n"
	       "  -p, --pretty       Prettify the output json\n"
	       "  -s, --suffix       Output suffix, if left out then stdout is used\n"
	       "  -v, --variable     Set key=value variable\n"
	       "  -h, --help         Show this help and exit\n"
	       "  -V, --version      Output version information\n",
	       PROGRAM_NAME);
}

static struct option const long_options[] = {
	{"version", no_argument, NULL, 'V'},
	{"help", no_argument, NULL, 'h'},
	{"suffix", required_argument, NULL, 's'},
	{"pretty", no_argument, NULL, 'p'},
	{"variable", required_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

char short_options[] = "ps:v:hV";

int main(int argc, char **argv)
{
	char *suffix = NULL, *tmpbuf, *varkey, *infile;
	size_t suflen = 0;
	int ch, argind, pretty = 0;
	jm_object_t *vars = NULL;

	if (!(vars = jm_newObject()))
		goto err;

	while ((ch = getopt_long(argc, argv, short_options, long_options,
				 NULL)) != -1) {
		switch (ch) {
		case 'V':
			print_version();
			goto err;
			break;
		case 'h':
			print_help();
			goto err;
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
			tmpbuf = strchr(varkey, '=');
			if (!tmpbuf) {
				print_usage();
				goto err;
			}
			*tmpbuf++ = '\0';
			jm_moveInto(vars, varkey, jm_newString(tmpbuf));
			free(varkey);
			break;
		default:
			print_usage();
			goto err;
		}
	}

	argind = optind;
	infile = "-";

	do {
		jm_object_t *root = NULL, *out = NULL;

		if (argind < argc)
			infile = argv[argind];

		if (!(root = jm_parseFile(infile)))
			goto err;


		if (!(out = jm_merge(root, vars))) {
			jm_free(root);
			goto err;
		}
		jm_free(root);

		if (strcmp(infile, "-") != 0 && suffix) {
			char *outfile = NULL;
			size_t inlen = strlen(infile);
			if (!(outfile = malloc(inlen + suflen + 1)))
				continue;
			memcpy(outfile, infile, inlen);
			memcpy(outfile + inlen, suffix, suflen + 1);
			jm_serialize(outfile, out, pretty ? JM_PRETTY : 0);
			free(outfile);
		} else {
			jm_serialize("-", out, pretty ? JM_PRETTY : 0);
		}

		jm_free(out);
	} while (++argind < argc);

	jm_free(vars);
	exit(EXIT_SUCCESS);

      err:
	jm_free(vars);
	exit(EXIT_FAILURE);
}
