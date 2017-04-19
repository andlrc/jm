#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jm.h"
#define MAX_INDENT 32

static char *indent(int depth)
{
	char *tabs = NULL, *rettabs = NULL;
	int max = depth > MAX_INDENT ? MAX_INDENT : depth;

	if ((tabs = malloc(max + 1)) == NULL)
		return NULL;
	rettabs = tabs;

	while (max--)
		*tabs++ = '\t';

	*tabs = '\0';

	return rettabs;
}

static char *escape(char *string)
{
	char *buff = NULL, *retbuff = NULL;
	size_t buff_size = 256;

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;

	retbuff = buff;

	*buff++ = '"';

	while (*string != '\0') {
		*buff++ = *string++;

		/* At most two bytes are written at once */
		if (buff_size - 1 <= (size_t) (buff - retbuff)) {
			char *temp;
			buff_size *= 2;
			if ((temp = realloc(retbuff, buff_size)) == NULL)
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
	case jm_type_unknown:
		fprintf(stderr,
			"%s: Cannot serialize type UNKNOWN\n",
			PROGRAM_NAME);
		return 1;
		break;

	case jm_type_object:
		fprintf(outfh, "{");
		isFirst = 1;
		next = node->firstChild;
		while (next != NULL) {
			if (!isFirst)
				fprintf(outfh, ",");
			else
				isFirst = 0;

			escaped = escape(next->name);

			if (flags & JM_PRETTY) {
				char *ind = indent(depth + 1);
				fprintf(outfh, "\n%s%s: ", ind, escaped);
				free(ind);
			} else {
				fprintf(outfh, "%s:", escaped);
			}
			free(escaped);
			serialize(outfh, next, flags, depth + 1);
			next = next->nextSibling;
		}
		if (!isFirst && flags & JM_PRETTY) {
			char *ind = indent(depth);
			fprintf(outfh, "\n%s}", ind);
			free(ind);
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
			while (next != NULL) {
				if (flags & JM_PRETTY) {
					char *ind = indent(depth + 1);
					fprintf(outfh, "%s", ind);
					free(ind);
				}
				serialize(outfh, next, flags, depth + 1);
				if (next->nextSibling)
					fprintf(outfh, ",");
				if (flags & JM_PRETTY)
					fprintf(outfh, "\n");
				next = next->nextSibling;
			}
			if (flags & JM_PRETTY) {
				char *ind = indent(depth);
				fprintf(outfh, "%s", ind);
				free(ind);
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
	if (fh == NULL) {
		fprintf(stderr, "%s: cannot create '%s'\n", PROGRAM_NAME,
			file);
		goto err;
	}
	if (node == NULL)
		goto err;

	int ret = serialize(fh, node, flags, 0);
	fprintf(fh, "\n");
	if (fh != stdout)
		fclose(fh);
	return ret;

      err:
	if (fh != NULL && fh != stdout)
		fclose(fh);
	return 1;
}