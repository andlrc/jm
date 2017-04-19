#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include "jx.h"

#define MAX_FILES 16

static int merge(jx_object_t * dest, jx_object_t * src);

static int template(char *dest, char *src, jx_object_t * vars)
{
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

			while (*keyNode->value != '\0')
				*dest++ = *keyNode->value++;
		} else {
			*dest++ = *src++;
		}
	}
	*dest++ = '\0';
	return 0;
}

static int mergeArray(jx_object_t * dest, jx_object_t * src)
{
	int ret = 0;

	/* Iterate array and call recursive */
	jx_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	int prependIndex = 0;

	/* Append, prepend and insert should happen after everyting else */
	struct insert_s {
		enum insertType_e {
			jx_insertType_append,
			jx_insertType_prepend,
			jx_insertType_insert
		} type;
		int prependIndex;
		jx_object_t *indicator;
		jx_object_t *node;
		struct insert_s *next;
	} *insert = NULL, *lastInsert = NULL, *firstInsert = NULL;

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jx_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	srcNext = src->firstChild;
	destNext = dest->firstChild;
	while (srcNext != NULL) {
		srcNext2 = srcNext->nextSibling;

		if (srcNext->type == jx_type_object) {
			if (srcNext->indicators->append != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->append;
				if (ind->type != jx_type_literal
				    || strcmp(ind->value, "true") != 0)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jx_insertType_append;
				insert->indicator = ind;
				insert->node = srcNext;
				insert->next = NULL;
				if (lastInsert == NULL)
					firstInsert = insert;
				else
					lastInsert->next = insert;
				lastInsert = insert;
				goto cont;
			}

			if (srcNext->indicators->prepend != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->prepend;
				if (ind->type != jx_type_literal
				    || strcmp(ind->value, "true") != 0)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jx_insertType_prepend;
				insert->prependIndex = prependIndex++;
				insert->indicator = ind;
				insert->node = srcNext;
				insert->next = NULL;
				if (lastInsert == NULL)
					firstInsert = insert;
				else
					lastInsert->next = insert;
				lastInsert = insert;
				goto cont;
			}

			if (srcNext->indicators->insert != NULL) {
				jx_object_t *ind =
				    srcNext->indicators->insert;
				if (ind->type != jx_type_literal)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jx_insertType_insert;
				insert->indicator = ind;
				insert->node = srcNext;
				insert->next = NULL;
				if (lastInsert == NULL)
					firstInsert = insert;
				else
					lastInsert->next = insert;
				lastInsert = insert;
				goto cont;
			}
		}

		if (destNext == NULL) {
			if (jx_arrayPush(dest, srcNext))
				goto errmov;
		} else {
			if ((ret = merge(destNext, srcNext)))
				goto cleanup;

			destNext = destNext->nextSibling;
		}
	      cont:
		srcNext = srcNext2;
	}

	insert = firstInsert;
	while (insert != NULL) {
		jx_object_t *ind = insert->indicator, *value = NULL;
		if (insert->node->type == jx_type_object
		    && insert->node->indicators->value) {
			value = insert->node->indicators->value;
			insert->node->indicators->value = NULL;
		} else {
			value = insert->node;
		}
		switch (insert->type) {
		case jx_insertType_append:
			if (jx_arrayPush(dest, value))
				goto errmov;
			break;
		case jx_insertType_prepend:
			if (jx_arrayInsertAt
			    (dest, insert->prependIndex, value))
				goto errmov;
			break;
		case jx_insertType_insert:
			if (jx_arrayInsertAt
			    (dest, atoi(ind->value), value))
				goto errmov;
			break;
		}
		insert = insert->next;
	}

	ret = 0;
	goto cleanup;

      errind:
	fprintf(stderr, "json_merger: Error in ARRAY indicator\n");
	ret = 1;
      errmov:
	fprintf(stderr, "json_merger: Error pushing ARRAY\n");
	ret = 1;
      cleanup:
	insert = firstInsert;
	while (insert != NULL) {
		lastInsert = insert->next;
		free(insert);
		insert = lastInsert;
	}

	return ret;
}

static int mergeObject(jx_object_t * dest, jx_object_t * src)
{
	int ret = 0;

	/* Check indicators */
	if (src->indicators->match != NULL) {
		jx_object_t *match = src->indicators->match, *tmp = NULL;

		/* TODO: dest could be null, we should really send parent */
		/* An object inside an array queries from the array */
		if (dest->parent == NULL
		    || dest->parent->type == jx_type_object)
			tmp = dest;
		else
			tmp = dest->parent;

		if ((dest = jx_query(tmp, match->value)) == NULL) {
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
		case jx_type_string:
			/* Override single properpy */
			jx_free(jx_locate(dest, override->value));
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->value) {
		jx_object_t *value = src->indicators->value, *tmp = NULL;

		tmp = value;
		src->indicators->value = NULL;
		/* TODO: This can be a root node */
		if (src->parent)
			jx_free(src);
		src = tmp;
		return merge(dest, src);
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
		else if ((ret = merge(destNext, srcNext)))
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

static jx_object_t *recurseMerge(jx_object_t * node, jx_object_t * vars)
{
	jx_object_t *dest = NULL;

	if (node->indicators && node->indicators->extends) {
		jx_object_t *next = node->indicators->extends->firstChild,
		    *tmp = NULL, *src = NULL;

		while (next != NULL) {

			char olddir[PATH_MAX], *dir = NULL;
			dir = strdup(node->filename);
			getcwd(olddir, PATH_MAX);
			dirname(dir);
			if (strcmp(dir, node->filename) != 0)
				chdir(dir);
			free(dir);

			if ((tmp = jx_parseFile(next->value)) == NULL) {
				jx_free(dest);
				chdir(olddir);
				return NULL;
			}

			if ((src = recurseMerge(tmp, vars)) == NULL) {
				jx_free(tmp);
				jx_free(dest);
				chdir(olddir);
				return NULL;
			}

			chdir(olddir);

			jx_free(tmp);
			if (dest == NULL)
				dest = src;
			else {
				merge(dest, src);
				jx_free(src);
			}

			next = next->nextSibling;
		}

		if (merge(dest, node)) {
			free(dest);
			return NULL;
		}
	} else {
		dest = node->type == jx_type_array ? jx_newArray()
		    : jx_newObject();

		if (merge(dest, node)) {
			free(dest);
			return NULL;
		}
	}

	return dest;
}

/* Caller should never free dest, but free the return value */
jx_object_t *jx_merge(jx_object_t * dest, jx_object_t * vars)
{
	return recurseMerge(dest, vars);
}
