#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jm.h"
#define MAX_INDENT 32

static char *indent(int depth)
{
	static char tabs[MAX_INDENT + 1];
	int max, i;
	max = depth > MAX_INDENT ? MAX_INDENT : depth;

	for (i = 0; i < max; i++)
		tabs[i] = '\t';

	tabs[i] = '\0';

	return tabs;
}

static char *escape(char *string)
{
	char *buff = NULL, *retbuff = NULL;
	size_t buffsize = 256;

	if (!(buff = malloc(buffsize)))
		return NULL;

	retbuff = buff;

	*buff++ = '"';

	while (*string != '\0') {
		*buff++ = *string++;

		/* At most two bytes are written at once */
		if (buffsize - 1 <= (size_t) (buff - retbuff)) {
			char *temp;
			buffsize *= 2;
			if (!(temp = realloc(retbuff, buffsize)))
				goto err;
			buff = temp + (buff - retbuff);
			retbuff = temp;
		}
	}

	*buff++ = '"';
	*buff++ = '\0';

	return retbuff;
      err:
	free(retbuff);
	return NULL;
}

static int serialize(FILE * outfh, jm_object_t * node, int flags,
		     int depth)
{
	char *escaped;
	int isFirst;
	jm_object_t *next;

	switch (node->type) {
	case jm_type_lid:
	case jm_type_lprepend:
	case jm_type_lappend:
	case jm_type_linsert:
	case jm_type_ldelete:
	case jm_type_loverride:
	case jm_type_lmatch:
		fprintf(stderr, "%s: cannot serialize INDICATOR nodes\n",
			PROGRAM_NAME);
		return 1;
		break;
	case jm_type_object:
		fprintf(outfh, "{");
		isFirst = 1;
		next = node->firstChild;
		while (next) {
			if (!isFirst)
				fprintf(outfh, ",");
			else
				isFirst = 0;

			escaped = escape(next->name);

			if (flags & JM_PRETTY) {
				fprintf(outfh, "\n%s%s: ",
					indent(depth + 1), escaped);
			} else {
				fprintf(outfh, "%s:", escaped);
			}
			free(escaped);
			serialize(outfh, next, flags, depth + 1);
			next = next->nextSibling;
		}
		if (!isFirst && flags & JM_PRETTY) {
			fprintf(outfh, "\n%s}", indent(depth));
		} else {
			fprintf(outfh, "}");
		}
		break;

	case jm_type_array:
		fprintf(outfh, "[");
		next = node->firstChild;
		if (next) {
			if (flags & JM_PRETTY)
				fprintf(outfh, "\n");
			while (next) {
				if (flags & JM_PRETTY) {
					fprintf(outfh, "%s",
						indent(depth + 1));
				}
				serialize(outfh, next, flags, depth + 1);
				if (next->nextSibling)
					fprintf(outfh, ",");
				if (flags & JM_PRETTY)
					fprintf(outfh, "\n");
				next = next->nextSibling;
			}
			if (flags & JM_PRETTY) {
				fprintf(outfh, "%s", indent(depth));
			}
		}
		fprintf(outfh, "]");
		break;

	case jm_type_string:
		escaped = escape(node->value);
		fprintf(outfh, "%s", escaped);
		free(escaped);
		break;

	case jm_type_literal:
		fprintf(outfh, "%s", node->value);
		break;
	}

	return 0;
}

int jm_serialize(char *file, jm_object_t * node, int flags)
{
	FILE *fh = strcmp(file, "-") == 0 ? stdout : fopen(file, "wb");
	if (!fh) {
		fprintf(stderr, "%s: cannot create '%s'\n", PROGRAM_NAME,
			file);
		goto err;
	}
	if (!node)
		goto err;

	int ret = serialize(fh, node, flags, 0);
	fprintf(fh, "\n");
	if (fh != stdout)
		fclose(fh);
	return ret;

      err:
	if (fh && fh != stdout)
		fclose(fh);
	return 1;
}
