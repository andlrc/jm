#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include "jm.h"

#define MAX_FILES 16

static int merge(jm_object_t * dest, jm_object_t * src);

static int template(char *dest, char *src, jm_object_t * vars)
{
	while (*src != '\0') {
		if (*src == '$') {
			char key[256], *val;
			size_t y = 0;
			jm_object_t *keyNode = NULL;
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

			if ((keyNode = jm_locate(vars, key)) == NULL)
				return 1;

			val = keyNode->value;

			while (*val != '\0')
				*dest++ = *val++;
		} else {
			*dest++ = *src++;
		}
	}
	*dest++ = '\0';
	return 0;
}

static int mergeArray(jm_object_t * dest, jm_object_t * src)
{
	int ret = 0;

	/* Iterate array and call recursive */
	jm_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	int prependIndex = 0;

	/* Append, prepend and insert should happen after everyting else */
	struct insert_s {
		enum insertType_e {
			jm_insertType_append,
			jm_insertType_prepend,
			jm_insertType_insert
		} type;
		int prependIndex;
		jm_object_t *indicator;
		jm_object_t *node;
		struct insert_s *next;
	} *insert = NULL, *lastInsert = NULL, *firstInsert = NULL;

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jm_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	srcNext = src->firstChild;
	destNext = dest->firstChild;
	while (srcNext != NULL) {
		srcNext2 = srcNext->nextSibling;

		if (srcNext->type == jm_type_object) {
			if (srcNext->indicators->append != NULL) {
				jm_object_t *ind =
				    srcNext->indicators->append;
				if (ind->type != jm_type_literal
				    || strcmp(ind->value, "true") != 0)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jm_insertType_append;
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
				jm_object_t *ind =
				    srcNext->indicators->prepend;
				if (ind->type != jm_type_literal
				    || strcmp(ind->value, "true") != 0)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jm_insertType_prepend;
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
				jm_object_t *ind =
				    srcNext->indicators->insert;
				if (ind->type != jm_type_literal)
					goto errind;
				if ((insert =
				     malloc(sizeof(struct insert_s))) ==
				    NULL)
					goto errind;
				insert->type = jm_insertType_insert;
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
			if (jm_arrayPush(dest, srcNext))
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
		jm_object_t *ind = insert->indicator, *value = NULL;
		if (insert->node->type == jm_type_object
		    && insert->node->indicators->value) {
			value = insert->node->indicators->value;
			insert->node->indicators->value = NULL;
		} else {
			value = insert->node;
		}
		switch (insert->type) {
		case jm_insertType_append:
			if (jm_arrayPush(dest, value))
				goto errmov;
			break;
		case jm_insertType_prepend:
			if (jm_arrayInsertAt
			    (dest, insert->prependIndex, value))
				goto errmov;
			break;
		case jm_insertType_insert:
			if (jm_arrayInsertAt
			    (dest, atoi(ind->value), value))
				goto errmov;
			break;
		}
		insert = insert->next;
	}

	ret = 0;
	goto cleanup;

      errind:
	fprintf(stderr, "%s: Error in ARRAY indicator\n", PROGRAM_NAME);
	ret = 1;
      errmov:
	fprintf(stderr, "%s: Error pushing ARRAY\n", PROGRAM_NAME);
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

static int mergeObject(jm_object_t * dest, jm_object_t * src)
{
	int ret = 0;

	/* Check indicators */
	if (src->indicators->match != NULL) {
		jm_object_t *match = src->indicators->match, *tmp = NULL;

		/* TODO: dest could be null, we should really send parent */
		/* An object inside an array queries from the array */
		if (dest->parent == NULL
		    || dest->parent->type == jm_type_object)
			tmp = dest;
		else
			tmp = dest->parent;

		if ((dest = jm_query(tmp, match->value)) == NULL) {
			fprintf(stderr,
				"%s: unrecognized or non matching selector '%s'\n",
				PROGRAM_NAME, match->value);
			return 1;
		}
	}

	if (src->indicators->move != NULL) {
		jm_object_t *move = src->indicators->move;

		switch (move->type) {
		case jm_type_literal:
			/* Move object to index */
			/* TODO: dest could be null, we should really send parent */
			return jm_arrayInsertAt(dest->parent,
						atoi(move->value), dest);
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->delete != NULL) {
		jm_object_t *delete = src->indicators->delete, *next =
		    NULL;

		switch (delete->type) {
		case jm_type_literal:
			/* Delete dest property */
			if (strcmp(delete->value, "true") == 0) {
				jm_free(dest);
				return 0;
				break;
			}
		case jm_type_array:
			/* Delete properties listed in array */
			next = delete->firstChild;
			while (next != NULL) {
				if (next->type != jm_type_string)
					goto errind;

				jm_free(jm_locate(dest, next->value));
				next = next->nextSibling;
			}
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->override != NULL) {
		jm_object_t *override = src->indicators->override, *next =
		    NULL;
		switch (override->type) {
		case jm_type_literal:
			/* Move over dest property */
			if (strcmp(override->value, "true") == 0) {
				if (jm_moveOver(dest, src))
					goto errmov;
				return 0;
			}
			break;
		case jm_type_array:
			/* Override properties listed in array.
			 * Just delete all mentioned properties, that should do
			 */
			next = override->firstChild;
			while (next != NULL) {
				if (next->type != jm_type_string)
					goto errind;

				jm_free(jm_locate(dest, next->value));
				next = next->nextSibling;
			}
			break;
		case jm_type_string:
			/* Override single properpy */
			jm_free(jm_locate(dest, override->value));
			break;
		default:
			goto errind;
			break;
		}
	}

	if (src->indicators->value) {
		jm_object_t *value = src->indicators->value, *tmp = NULL;

		tmp = value;
		src->indicators->value = NULL;
		/* TODO: This can be a root node */
		if (src->parent)
			jm_free(src);
		src = tmp;
		return merge(dest, src);
	}

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jm_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	/* Check indicators, and if nothing else then iterate properties and
	 * call recursive */
	jm_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	srcNext = src->firstChild;

	while (srcNext != NULL) {
		srcNext2 = srcNext->nextSibling;

		if ((destNext = jm_locate(dest, srcNext->name)) == NULL)
			jm_moveInto(dest, srcNext->name, srcNext);
		else if ((ret = merge(destNext, srcNext)))
			return ret;

		srcNext = srcNext2;
	}

	return 0;

      errind:
	fprintf(stderr, "%s: Error in OBJECT indicator\n", PROGRAM_NAME);
	return 1;
      errmov:
	fprintf(stderr, "%s: Error moving into OBJECT\n", PROGRAM_NAME);
	return 1;
}

static int merge(jm_object_t * dest, jm_object_t * src)
{
	switch (src->type) {
	case jm_type_unknown:
		fprintf(stderr,
			"%s: cannot merge UNKNOWN type\n", PROGRAM_NAME);
		return 1;
		break;
	case jm_type_object:
		return mergeObject(dest, src);
		break;
	case jm_type_array:
		return mergeArray(dest, src);
		break;
	case jm_type_string:	/* Copy string over */
		jm_moveOver(dest, src);
		break;
	case jm_type_literal:	/* Copy literal over */
		jm_moveOver(dest, src);
		break;
	}

	return 0;
}

static jm_object_t *recurseMerge(jm_object_t * node, jm_object_t * vars)
{
	jm_object_t *dest = NULL;

	if (node->indicators && node->indicators->extends) {
		jm_object_t *next = node->indicators->extends->firstChild,
		    *tmp = NULL, *src = NULL;

		while (next != NULL) {

			char olddir[PATH_MAX], filename[PATH_MAX],
			    *dir = NULL;
			dir = strdup(node->filename);
			if (!getcwd(olddir, PATH_MAX)) {
				free(dir);
				return NULL;
			}
			dirname(dir);
			if (strcmp(dir, node->filename) != 0 && chdir(dir)) {
				free(dir);
				return NULL;
			}
			free(dir);

			template(filename, next->value, vars);
			if ((tmp = jm_parseFile(filename)) == NULL) {
				jm_free(dest);
				return NULL;
			}

			if ((src = recurseMerge(tmp, vars)) == NULL) {
				jm_free(tmp);
				jm_free(dest);
				return NULL;
			}

			if (chdir(olddir)) {
				jm_free(tmp);
				jm_free(dest);
				return NULL;
			}

			jm_free(tmp);
			if (!dest) {
				dest = src;
			} else {
				merge(dest, src);
				jm_free(src);
			}

			next = next->nextSibling;
		}
	}

	if (!dest)
		dest = node->type == jm_type_array ? jm_newArray()
		    : jm_newObject();

	if (merge(dest, node)) {
		free(dest);
		return NULL;
	}

	return dest;
}

/* Caller should never free dest, but free the return value */
jm_object_t *jm_merge(jm_object_t * dest, jm_object_t * vars)
{
	return recurseMerge(dest, vars);
}
