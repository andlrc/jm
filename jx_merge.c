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
			char *filename = next->value;
			if ((mergeTree->extends[size++] =
			     resolve(jx_parseFile(filename),
				     vars)) == NULL)
				goto err;
			next = next->nextSibling;
		}
		if (chdir(olddir))
			goto err;
		free(dir);
	}

	mergeTree->size = size;
	return mergeTree;

      err:
	fprintf(stderr,
		"json_merger: error happened while merging, aborting\n");
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
