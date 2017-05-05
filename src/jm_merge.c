#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include "jm.h"

#define MAX_FILES 16

static int merge(jm_object_t * dest, jm_object_t * src, jm_object_t * ids);
static jm_object_t *recurseMerge(jm_object_t * node, jm_object_t * vars);

static int template(char *dest, char *src, jm_object_t * vars)
{
	char key[256], *pkey = NULL, *pval = NULL;
	while (*src != '\0') {
		if (*src == '$') {
			pkey = key;
			jm_object_t *keyNode = NULL;
			src++;
			if (*src == '{') {
				src++;
				while (*src != '}') {
					if (*src == '\0')
						return 1;
					*pkey++ = *src++;
				}
				src++;
			} else {
				while ((*src >= 'a' && *src <= 'z')
				       || (*src >= 'A' && *src <= 'Z')
				       || (*src >= '0' && *src <= '9')
				       || *src == '_') {
					*pkey++ = *src++;
				}
			}

			*pkey = '\0';
			if ((keyNode = jm_locate(vars, key))) {
				pval = keyNode->value;
				while (*pval != '\0')
					*dest++ = *pval++;
			}
		} else {
			*dest++ = *src++;
		}
	}
	*dest++ = '\0';
	return 0;
}

static int mergeArray(jm_object_t * dest, jm_object_t * src,
		      jm_object_t * ids)
{
	/* Iterate array and call recursive */
	int ret = 0;
	jm_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	struct inserter_s {
		jm_object_t *src;
		jm_object_t *inserter;
		struct inserter_s *next;
	} *finserter = NULL, *inserter;

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jm_moveOver(dest, src))
			goto errmov;
		return 0;
	}

	srcNext = src->firstChild;
	destNext = dest->firstChild;
	while (srcNext) {
		srcNext2 = srcNext->nextSibling;

		/* inserters happen after the array is merged */
		if (srcNext->type == jm_type_object
		    && srcNext->indicators->inserter) {
			if (!finserter) {
				finserter =
				    malloc(sizeof(struct inserter_s));
				inserter = finserter;
			} else {
				inserter->next =
				    malloc(sizeof(struct inserter_s));
				inserter = inserter->next;
			}
			inserter->src = srcNext;
			inserter->inserter = srcNext->indicators->inserter;
			inserter->next = NULL;
		} else if (!destNext) {
			if (jm_arrayPush(dest, srcNext))
				goto errmov;
		} else {
			if ((ret = merge(destNext, srcNext, ids)))
				goto err;
			destNext = destNext->nextSibling;
		}

		srcNext = srcNext2;
	}

	if (finserter) {
		int pindex = 0;	/* Prepend index */
		jm_object_t *val = NULL;
		inserter = finserter;
		while (inserter) {
			/* Map @value */
			val = inserter->src;
			if (val->type == jm_type_object
			    && val->indicators->value) {
				val = val->indicators->value;
				inserter->src->indicators->value = NULL;
			}

			switch (inserter->inserter->type) {
			case jm_type_lappend:
				jm_arrayPush(dest, val);
				break;
			case jm_type_lprepend:
				jm_arrayInsertAt(dest, pindex++, val);
				break;
			case jm_type_linsert:
				jm_arrayInsertAt(dest,
						 atoi(inserter->inserter->
						      value), val);
				break;
			default:
				goto err;
				break;
			}
			finserter = inserter->next;
			free(inserter);
			inserter = finserter;
		}
	}

	return 0;

      errmov:
	fprintf(stderr, "%s: Error pushing ARRAY\n", PROGRAM_NAME);
      err:
	if (finserter) {
		inserter = finserter;
		while (inserter) {
			finserter = inserter->next;
			free(inserter);
			inserter = finserter;
		}
	}

	return 1;
}


static int mergeObject(jm_object_t * dest, jm_object_t * src,
		       jm_object_t * ids)
{
	int ret, didself = 0;	/* It's possible to delete and override the dest node,
				   in that case execution should stop just after
				   processing matchers */
	jm_object_t *matchers = NULL, *next = NULL, *tmpnode = NULL;
	matchers = src->indicators->matchers;
	if (matchers) {
		next = matchers->firstChild;
		while (next) {
			switch (next->type) {
			case jm_type_lmatch:
				/* An object inside an array queries from the array */
				if (dest->parent == NULL
				    || dest->parent->type == jm_type_object)
					tmpnode = dest;
				else
					tmpnode = dest->parent;

				if (!(dest = jm_query(tmpnode,
						      next->value, ids)))
					return 1;
				if (src->indicators->move) {
					jm_object_t *move =
					    src->indicators->move;
					jm_arrayInsertAt(dest->parent,
							 atoi(move->value),
							 dest);
				}
				break;
			case jm_type_ldelete:
				if (!(tmpnode = jm_query(dest,
							 next->value, ids)))
					return 1;
				if (dest == tmpnode)
					didself = 1;
				if (!tmpnode->parent) {
					fprintf(stderr,
						"%s: cannot delete a ROOT node\n",
						PROGRAM_NAME);
					return 1;
				}
				jm_free(tmpnode);
				break;
			case jm_type_loverride:
				if (!(tmpnode = jm_query(dest,
							 next->value, ids)))
					return 1;
				if (!tmpnode->parent) {
					fprintf(stderr,
						"%s: cannot override a ROOT node\n",
						PROGRAM_NAME);
					return 1;
				}
				if (tmpnode == dest) {
					jm_moveOver(dest, src);
					didself = 1;
				} else {
					jm_free(tmpnode);
				}
				break;
			default:
				return 1;	/* This should never happen */
				break;
			}
			next = next->nextSibling;
		}
	}

	if (didself)
		return 0;

	if (src->indicators->value) {
		jm_object_t *value = src->indicators->value, *tmp = NULL;

		tmp = value;
		src->indicators->value = NULL;
		/* TODO: This can be a root node */
		if (src->parent)
			jm_free(src);
		src = tmp;
		ret = merge(dest, src, ids);
		jm_free(src);
		return ret;
	}

	/* Move src over dest */
	if (dest->type != src->type) {
		if (jm_moveOver(dest, src)) {
			fprintf(stderr, "%s: Error moving into OBJECT\n",
				PROGRAM_NAME);
			return 1;
		}
		return 0;
	}

	/* Check indicators, and if nothing else then iterate properties and
	 * call recursive */
	jm_object_t *srcNext = NULL, *srcNext2 = NULL, *destNext = NULL;

	srcNext = src->firstChild;

	while (srcNext) {
		srcNext2 = srcNext->nextSibling;

		if (!(destNext = jm_locate(dest, srcNext->name)))
			jm_moveInto(dest, srcNext->name, srcNext);
		else if ((ret = merge(destNext, srcNext, ids)))
			return ret;

		srcNext = srcNext2;
	}

	return 0;
}

static int merge(jm_object_t * dest, jm_object_t * src, jm_object_t * ids)
{
	switch (src->type) {
	case jm_type_lid:
	case jm_type_lappend:
	case jm_type_lprepend:
	case jm_type_linsert:
	case jm_type_ldelete:
	case jm_type_loverride:
	case jm_type_lmatch:
		fprintf(stderr,
			"%s: cannot merge INDICATORS nodes\n",
			PROGRAM_NAME);
		return 1;
		break;
	case jm_type_object:
		return mergeObject(dest, src, ids);
		break;
	case jm_type_array:
		return mergeArray(dest, src, ids);
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

static jm_object_t *initMerge(jm_object_t * node, char *path,
			      jm_object_t * vars)
{
	jm_object_t *tmp = NULL, *src = NULL;
	char olddir[PATH_MAX], filename[PATH_MAX], *dir = NULL;
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

	template(filename, path, vars);
	if (!(tmp = jm_parseFile(filename))) {
		return NULL;
	}

	if (!(src = recurseMerge(tmp, vars))) {
		jm_free(tmp);
		return NULL;
	}

	if (chdir(olddir)) {
		jm_free(tmp);
		return NULL;
	}

	jm_free(tmp);
	return src;
}

static jm_object_t *recurseMerge(jm_object_t * node, jm_object_t * vars)
{
	jm_object_t *dest = NULL;

	if (node->indicators && node->indicators->extends) {
		jm_object_t *extends = NULL, *next = NULL, *src = NULL;
		extends = node->indicators->extends;
		switch (extends->type) {
		case jm_type_string:
			if (!(dest = initMerge(node,
					       extends->value, vars))) {
				return NULL;
			}
			break;
		case jm_type_array:
			next = extends->firstChild;
			while (next) {
				if (!(src = initMerge(node,
						      next->value, vars))) {
					jm_free(dest);
					return NULL;
				}
				if (!dest) {
					dest = src;
				} else {
					merge(dest, src, dest->ids);
					/* Move over new ID's */
					jm_free(dest->ids);
					dest->ids = src->ids;
					src->ids = NULL;
					jm_free(src);
				}

				next = next->nextSibling;
			}
			break;
		default:
			return NULL;
			break;
		}
	}

	if (!dest)
		dest = node->type == jm_type_array ? jm_newArray()
		    : jm_newObject();

	if (merge(dest, node, dest->ids)) {
		jm_free(dest);
		return NULL;
	}
	/* Move over new ID's */
	if (dest->ids)
		jm_free(dest->ids);
	dest->ids = node->ids;
	node->ids = NULL;

	return dest;
}

/* Caller should never free dest, but free the return value */
jm_object_t *jm_merge(jm_object_t * dest, jm_object_t * vars)
{
	return recurseMerge(dest, vars);
}
