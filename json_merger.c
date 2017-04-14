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

	int i = 1;
	for (; i < argc; i++) {
		char *arg = argv[i];
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
				// TODO Add value:
				i++;
				// argv[++i];
				break;
			default:
				break;
			case '-':
				/* Fallthough */
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
		FILE *infh = fopen(infile, "rb"), *outfh = stdout;
		if (infh == NULL) {
			fprintf(stderr,
				"json_merger: cannot access '%s'\n",
				infile);
			return 1;
		}
		if (outsuffix != NULL) {
			char *outfile = calloc(sizeof(char),
					       strlen(infile) +
					       strlen(outsuffix) + 1);
			if (outfile == NULL) {
				fprintf(stderr,
					"json_merger: cannot allocate memory for output file\n");
				fclose(infh);
				return 1;
			}
			strcat(outfile, infile);
			strcat(outfile, outsuffix);
			outfh = fopen(outfile, "wb");
		}

		jx_object_t *root = jx_parseFile(infh);
		jx_serialize(outfh, root, pretty ? JX_PRETTY : 0);

		fclose(infh);
		if (outfh != stdout) {
			fclose(outfh);
		}
	}

	return 0;
}
