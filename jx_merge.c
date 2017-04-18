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

static int merge(jx_object_t * dest, jx_object_t * src);

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

static void freeTree(struct jx_mergeTree_s *mergeTree)
{
	struct jx_mergeTree_s **extends = NULL;
	int size;

	if (mergeTree != NULL) {
		extends = mergeTree->extends;
		size = mergeTree->size;

		for (int i = 0; i < size; i++)
			freeTree(extends[i]);

		free(extends);
		free(mergeTree);
	}
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

static int mergeArray(jx_object_t * dest, jx_object_t * src)
{
	int ret = 0;

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jx_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	/* Iterate array and call recursive */
	jx_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	srcNext = src->firstChild;
	destNext = dest->firstChild;
	while (srcNext != NULL) {
		srcNext2 = srcNext->nextSibling;

		if (srcNext->type == jx_type_object) {
			if (srcNext->indicators->append != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->append;
				if (ind->type != jx_type_literal)
					goto errind;
				if (strcmp(ind->value, "true") == 0) {
					if (jx_arrayPush(dest, srcNext))
						goto errmov;
					goto cont;
				}
			}

			if (srcNext->indicators->prepend != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->prepend;
				if (ind->type != jx_type_literal)
					goto errind;
				if (strcmp(ind->value, "true") == 0) {
					if (jx_arrayInsertAt
					    (dest, 0, srcNext))
						goto errmov;
					goto cont;
				}
			}

			if (srcNext->indicators->insert != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->insert;
				if (ind->type != jx_type_literal)
					goto errind;
				if (jx_arrayInsertAt
				    (dest, atoi(ind->value), srcNext))
					goto errmov;
				goto cont;
			}
		}

		if (destNext == NULL) {
			if (jx_arrayPush(dest, srcNext))
				goto errmov;
		} else {
			if ((ret = merge(destNext, srcNext)))
				return ret;

			destNext = destNext->nextSibling;
		}
	      cont:
		srcNext = srcNext2;
	}

	return 0;

      errind:
	fprintf(stderr, "json_merger: Error in ARRAY indicator\n");
	return 1;
      errmov:
	fprintf(stderr, "json_merger: Error pushing ARRAY\n");
	return 1;
}

static int mergeObject(jx_object_t * dest, jx_object_t * src)
{
	int ret = 0;

	/* Check indicators */
	if (src->indicators->match != NULL) {
		jx_object_t *match = src->indicators->match;

		/* TODO: dest could be null, we should really send parent */
		if ((dest = jx_query(dest->parent, match->value)) == NULL) {
			fprintf(stderr,
				"json_merger: unrecognized or non matching selector '%s'\n",
				match->value);
			return 1;
		}
	}

	if (src->indicators->move != NULL) {
		jx_object_t *move = src->indicators->move;

		switch (move->type) {
		case jx_type_literal:
			/* Move object to index */
			/* TODO: dest could be null, we should really send parent */
			return jx_arrayInsertAt(dest->parent,
						atoi(move->value), dest);
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->delete != NULL) {
		jx_object_t *delete = src->indicators->delete, *next =
		    NULL;

		switch (delete->type) {
		case jx_type_literal:
			/* Delete dest property */
			if (strcmp(delete->value, "true") == 0) {
				jx_free(dest);
				return 0;
				break;
			}
		case jx_type_array:
			/* Delete properties listed in array */
			next = delete->firstChild;
			while (next != NULL) {
				if (next->type != jx_type_string)
					goto errind;

				jx_free(jx_locate(dest, next->value));
				next = next->nextSibling;
			}
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->override != NULL) {
		jx_object_t *override = src->indicators->override, *next =
		    NULL;
		switch (override->type) {
		case jx_type_literal:
			/* Move over dest property */
			if (strcmp(override->value, "true") == 0) {
				if (jx_moveOver(dest, src))
					goto errmov;
				return 0;
			}
			break;
		case jx_type_array:
			/* Override properties listed in array.
			 * Just delete all mentioned properties, that should do
			 */
			next = override->firstChild;
			while (next != NULL) {
				if (next->type != jx_type_string)
					goto errind;

				jx_free(jx_locate(dest, next->value));
				next = next->nextSibling;
			}
			break;
		default:
			goto errind;
			break;
		}
	}

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jx_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	/* Check indicators, and if nothing else then iterate properties and
	 * call recursive */
	jx_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	srcNext = src->firstChild;

	while (srcNext != NULL) {
		srcNext2 = srcNext->nextSibling;

		if ((destNext = jx_locate(dest, srcNext->name)) == NULL)
			jx_moveInto(dest, srcNext->name, srcNext);
		else if ((ret =
			  merge(jx_locate(dest, srcNext->name), srcNext)))
			return ret;

		srcNext = srcNext2;
	}

	return 0;

      errind:
	fprintf(stderr, "json_merger: Error in OBJECT indicator\n");
	return 1;
      errmov:
	fprintf(stderr, "json_merger: Error moving into OBJECT\n");
	return 1;
}

static int merge(jx_object_t * dest, jx_object_t * src)
{
	switch (src->type) {
	case jx_type_unknown:
		fprintf(stderr,
			"json_merger: cannot merge UNKNOWN type\n");
		return 1;
		break;
	case jx_type_object:
		return mergeObject(dest, src);
		break;
	case jx_type_array:
		return mergeArray(dest, src);
		break;
	case jx_type_string:	/* Copy string over */
		jx_moveOver(dest, src);
		break;
	case jx_type_literal:	/* Copy literal over */
		jx_moveOver(dest, src);
		break;
	}

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

	dest = resolved[0];
	/* Two or more elements left to merge */
	for (int i = 1; i < size; i++) {
		src = resolved[i];
		merge(dest, src);
	}

	/* Merge unto root node */
	merge(dest, node);
	node = dest;

	for (int i = 1; i < size; i++)
		jx_free(resolved[i]);
	free(resolved);

	return node;
}

/* Caller should never free dest, but free the return value */
jx_object_t *jx_merge(jx_object_t * dest, jx_object_t * vars)
{
	struct jx_mergeTree_s *mergeTree = genTree(dest, vars);

	if (mergeTree == NULL)
		return NULL;

	jx_object_t *result = recurseMerge(mergeTree);
	freeTree(mergeTree);
	return result;
}
