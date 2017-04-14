#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include "jx.h"

static inline void print_help(void)
{
	printf("Usage: json_merger [OPTION] <file>\n\n"
	       "  -o           Output suffix, if left out then stdout is used\n"
	       "  -p           Prettify the output json\n"
	       "  -v           Set key=value variable\n"
	       "  -h           Show this help and exit\n");
}

int main(int argc, char **argv)
{
	int pretty = 0;		/* Pretty print */
	char *outsuffix = NULL;	/* Output suffix */
	char *arg = NULL, *key = NULL;
	jx_object_t *vars = NULL;
	int i = 1;

	if ((vars = jx_newObject()) == NULL) {
		return 1;
	}

	for (; i < argc; i++) {
		arg = argv[i];
		if (arg[0] == '-') {
			switch (arg[1]) {
			case 'h':
				print_help();
				return 0;
			case 'o':
				if (i + 1 == argc) {
					fprintf(stderr,
						"json_merger: missing argument for '-o'");
					return 1;
				}
				outsuffix = argv[++i];
			case 'p':
				pretty = 1;
				break;
			case 'v':
				if (i + 1 == argc) {
					fprintf(stderr,
						"json_merger: missing argument for '-v'");
					return 1;
				}

				arg = argv[++i];
				key = arg;
				while (*arg != '\0' && *arg != '=')
					arg++;

				if (*arg == '=')
					key[arg++ - key] = '\0';

				jx_moveInto(vars, key, jx_newString(arg));
				break;
			default:
				break;
			case '-':
				i++;
				goto files;
				break;
			case '\0':
				goto files;
				break;
			}
		} else {
			/* Must be a file then */
			break;
		}
	}
      files:

	for (; i < argc; i++) {
		char *infile = argv[i];
		char *outfile = NULL;

		jx_object_t *root = jx_parseFile(infile);
		if (root == NULL)
			continue;

		jx_merge(root, vars);

		if (outsuffix != NULL && strcmp(infile, "-") != 0) {
			outfile = calloc(sizeof(char),
					 strlen(infile) +
					 strlen(outsuffix) + 1);
			if (outfile == NULL) {
				fprintf(stderr,
					"json_merger: cannot allocate memory for output file\n");
				return 1;
			}
			strcat(outfile, infile);
			strcat(outfile, outsuffix);
		} else {
			outfile = strdup("-");
		}

		jx_serialize(outfile, root, pretty ? JX_PRETTY : 0);
		free(outfile);
		jx_free(root);
	}

	jx_free(vars);

	return 0;
}
