#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jx.h"
#define MAX_INDENT 32

static char *indent(int depth)
{
	char *tabs = NULL;
	int max = depth > MAX_INDENT ? MAX_INDENT : depth;

	if ((tabs = malloc(max)) == NULL)
		return NULL;

	for (int i = 0; i < max; i++)
		tabs[i] = '\t';

	tabs[max] = '\0';

	return tabs;
}

static char *escape(char *string)
{
	char *buff = NULL;
	size_t buff_size = 256;
	size_t i = 0;

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;

	buff[i++] = '"';

	while (*string != '\0') {
		switch (*string) {
		case '"':
			buff[i++] = '\\';
			buff[i++] = '"';
			break;
		case '\b':
			buff[i++] = '\\';
			buff[i++] = 'b';
			break;
		case '\f':
			buff[i++] = '\\';
			buff[i++] = 'f';
			break;
		case '\n':
			buff[i++] = '\\';
			buff[i++] = 'n';
			break;
		case '\r':
			buff[i++] = '\\';
			buff[i++] = 'r';
			break;
		case '\t':
			buff[i++] = '\\';
			buff[i++] = 't';
		default:
			buff[i++] = *string;
			break;
		}

		string++;

		/* At most two bytes are written at once */
		if (i + 1 >= buff_size) {
			buff_size *= 2;
			if (realloc(buff, buff_size) == NULL)
				goto err;
		}
	}

	buff[i++] = '"';
	buff[i] = '\0';

	return buff;
      err:
	free(buff);
	return NULL;
}

static int serialize(FILE * outfh, jx_object_t * node, int flags,
		     int depth)
{
	char *escaped;
	int isFirst;
	jx_object_t *next;

	switch (node->type) {
	case jx_type_unknown:
		fprintf(stderr,
			"json_merger: Cannot serialize type UNKNOWN\n");
		return 1;
		break;

	case jx_type_object:
		fprintf(outfh, "{");
		isFirst = 1;
		next = node->firstChild;

		while (next != NULL) {
			if (!isFirst)
				fprintf(outfh, ",");
			else
				isFirst = 0;
			if (flags & JX_PRETTY)
				fprintf(outfh, "\n");
			escaped = escape(next->name);
			if (flags & JX_PRETTY) {
				char *ind = indent(depth + 1);
				fprintf(outfh, "%s", ind);
				free(ind);
			}
			fprintf(outfh, "%s", escaped);
			free(escaped);
			fprintf(outfh, flags & JX_PRETTY ? ": " : ":");
			serialize(outfh, next, flags, depth + 1);
			next = next->nextSibling;
		}
		if (!isFirst) {
			if (flags & JX_PRETTY)
				fprintf(outfh, "\n");
		}
		if (flags & JX_PRETTY) {
			char *ind = indent(depth);
			fprintf(outfh, "%s", ind);
			free(ind);
		}
		fprintf(outfh, "}");
		break;

	case jx_type_array:
		fprintf(outfh, "[");
		if (flags & JX_PRETTY)
			fprintf(outfh, "\n");
		next = node->firstChild;
		while (next != NULL) {
			if (flags & JX_PRETTY) {
				char *ind = indent(depth + 1);
				fprintf(outfh, "%s", ind);
				free(ind);
			}
			serialize(outfh, next, flags, depth + 1);
			if (next->nextSibling)
				fprintf(outfh, ",");
			if (flags & JX_PRETTY)
				fprintf(outfh, "\n");
			next = next->nextSibling;
		}
		if (flags & JX_PRETTY) {
			char *ind = indent(depth);
			fprintf(outfh, "%s", ind);
			free(ind);
		}
		fprintf(outfh, "]");
		break;

	case jx_type_string:
		escaped = escape(node->value);
		fprintf(outfh, "%s", escaped);
		free(escaped);
		break;

	case jx_type_literal:
		fprintf(outfh, "%s", node->value);
		break;
	}

	return 0;
}

int jx_serialize(char *file, jx_object_t * node, int flags)
{
	FILE *fh = strcmp(file, "-") == 0 ? stdout : fopen(file, "wb");
	if (fh == NULL) {
		fprintf(stderr, "json_merger: cannot create '%s'\n", file);
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
