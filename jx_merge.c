#include <stdio.h>
#include <string.h>
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

static struct jx_mergeTree_s *genTree(jx_object_t * dest,
				      jx_object_t * vars)
{

	struct jx_mergeTree_s *mergeTree = NULL;
	int size = 0;
	jx_object_t *extends = NULL, *next = NULL;
	char *dir = NULL;

	if ((mergeTree = malloc(sizeof(struct jx_mergeTree_s))) == NULL)
		goto err;

	mergeTree->node = dest;
	mergeTree->extends = NULL;

	if (dest->type != jx_type_object)
		return mergeTree;

	extends = dest->indicators->extends;

	if (extends != NULL) {
		if ((mergeTree->extends =
		     malloc(sizeof(struct jx_mergeTree_s *) *
			    MAX_FILES)) == NULL)
			goto err;

		for (int i = 0; i < MAX_FILES; i++)
			mergeTree->extends[i] = NULL;

		char olddir[PATH_MAX], *dir = NULL;
		dir = strdup(dest->filename);
		if (getcwd(olddir, PATH_MAX) == NULL
		    || dirname(dir) == NULL)
			goto err;
		if (strcmp(dir, dest->filename) == 0) {
			free(dir);
			dir = strdup(".");
		}
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
			     genTree(obj, vars)) == NULL)
				goto err;
			next = next->nextSibling;
		}
		if (chdir(olddir))
			goto err;
	}

	mergeTree->size = size;
	free(dir);
	return mergeTree;

      err:
	/* TODO: A more graceful error handling should be made
	 * And 'mergeTree' should potential be freeded */
	exit(1);
	return NULL;
}

static int merge(jx_object_t * dest, jx_object_t * src)
{
	/* TODO: Merge objects */

	printf("dst: %s\n", dest->filename);
	printf("src: %s\n", src->filename);

	return 0;
}

static jx_object_t *recurseMerge(struct jx_mergeTree_s *mergeTree)
{
	/* mergeTree could look like this:
	 * { A .node = ptr, .size = 2, .extends = {
	 *     { B .node = ptr, .size = 3, .extends = {
	 *         { C .node = ptr },
	 *         { D .node = ptr .size = 2, .extends = {
	 *           { E .node = ptr }.
	 *           { F .node = ptr }
	 *         } }
	 *         { G .node = pre }
	 *       }
	 *     },
	 *     { H .node = ptr, .size = 3, .extends = {
	 *       { I .node = ptr },
	 *       { J .node = ptr },
	 *       { K .node = ptr } } } } }
	 *
	 * In the above case merging would happen like this:
	 * A < ((B < (C < ((D < (E < F)) < G))) < (H < (I < (J < K)))
	 *
	 * Where `<' means lvalue extends rvalue.
	 */

	jx_object_t *node = mergeTree->node, *src = NULL, *dest = NULL;
	struct jx_mergeTree_s **extends = mergeTree->extends;
	int size = mergeTree->size;
	jx_object_t **resolved = NULL;

	if (size == 0)
		return node;

	if ((resolved = malloc(sizeof(jx_object_t *) * size)) == NULL)
		return NULL;

	for (int i = 0; i < size; i++)
		resolved[i] = recurseMerge(extends[i]);

	/* Two or more elements left merge */
	for (int i = 0; i < size - 1; i++) {
		dest = resolved[i];
		src = resolved[i + 1];
		merge(src, dest);
	}

	/* Merge unto root node */
	merge(node, resolved[size - 1]);

	return node;
}

int jx_merge(jx_object_t * dest, jx_object_t * vars)
{
	struct jx_mergeTree_s *mergeTree = genTree(dest, vars);

	if (mergeTree == NULL || recurseMerge(mergeTree) == NULL)
		return 1;
	return 0;
}
