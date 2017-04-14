#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include "jx.h"

#define MAX_FILES 16

struct jx_mergeTree_s {
	jx_object_t *node;
	struct jx_mergeTree_s **extends;
	int size;
};

static int template(char *dest, char *src, jx_object_t * vars)
{
	size_t i = 0;
	while (*src != '\0') {
		if (*src == '$') {
			char key[256];
			size_t y = 0;
			jx_object_t *keyNode = NULL;
			src++;
			if (*src == '{') {
				src++;
				while (*src != '}') {
					if (*src == '\0')
						return 1;
					key[y++] = *src++;
				}
				src++;
			} else {
				while ((*src >= 'a' && *src <= 'z')
				       || (*src >= 'A' && *src <= 'Z')
				       || (*src >= '0' && *src <= '9')
				       || *src == '_') {
					key[y++] = *src++;
				}
			}

			key[y] = '\0';

			if ((keyNode = jx_locate(vars, key)) == NULL)
				return 1;

			for (size_t y = 0; keyNode->value[y] != '\0'; y++) {
				dest[i++] = keyNode->value[y];
			}
		} else {
			dest[i++] = *src++;
		}
	}
	return 0;
}

static struct jx_mergeTree_s *resolve(jx_object_t * dest,
				      jx_object_t * vars)
{

	struct jx_mergeTree_s *mergeTree = NULL;
	int size = 0;
	jx_object_t *extends = dest->indicators->extends, *next = NULL;

	if ((mergeTree = malloc(sizeof(struct jx_mergeTree_s))) == NULL)
		goto err;

	mergeTree->node = dest;

	if ((mergeTree->extends =
	     malloc(sizeof(struct jx_mergeTree_s *) * MAX_FILES)) == NULL)
		goto err;

	for (int i = 0; i < MAX_FILES; i++)
		mergeTree->extends[i] = NULL;

	if (extends != NULL) {
		char olddir[PATH_MAX], *dir = NULL;
		if (getcwd(olddir, PATH_MAX) == NULL
		    || (dir = dirname(dest->filename)) == NULL)
			goto err;
		if (chdir(dir))
			goto err;

		next = extends->firstChild;
		while (next != NULL) {
			char filename[PATH_MAX];
			jx_object_t *obj = NULL;
			if (template(filename, next->value, vars)) {
				fprintf(stderr,
					"json_merger: failed to template filename\n");
				goto err;
			}
			if ((obj = jx_parseFile(filename)) == NULL)
				goto err;
			if ((mergeTree->extends[size++] =
			     resolve(obj, vars)) == NULL)
				goto err;
			next = next->nextSibling;
		}
		if (chdir(olddir))
			goto err;
	}

	mergeTree->size = size;
	return mergeTree;

      err:
	exit(1);
	return NULL;
}

int jx_merge(jx_object_t * dest, jx_object_t * vars)
{
	struct jx_mergeTree_s *mergeTree = resolve(dest, vars);

	if (mergeTree != NULL)
		return 0;
	return 1;
}
