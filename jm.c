#include <string.h>
#include <stdlib.h>
#include "jm.h"

jm_object_t *jm_newNode(enum jm_type_e type)
{
	jm_object_t *node = NULL;

	if ((node = malloc(sizeof(jm_object_t))) == NULL) {
		return NULL;
	}

	node->type = type;
	node->name = NULL;
	node->indicators = NULL;
	node->value = NULL;
	node->firstChild = NULL;
	node->lastChild = NULL;
	node->nextSibling = NULL;
	node->parent = NULL;
	node->filename = NULL;

	return node;
}

jm_object_t *jm_newObject(void)
{
	jm_object_t *node = NULL;
	struct jm_indicators_s *indicators = NULL;

	if ((node = jm_newNode(jm_type_object)) == NULL)
		return NULL;

	if ((indicators = malloc(sizeof(struct jm_indicators_s))) == NULL) {
		free(node);
		return NULL;
	}

	indicators->extends = NULL;
	indicators->move = NULL;
	indicators->value = NULL;
	indicators->inserter = NULL;
	indicators->matchers = NULL;

	node->indicators = indicators;

	return node;
}

jm_object_t *jm_newArray(void)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode(jm_type_array)) == NULL)
		return NULL;

	return node;
}

jm_object_t *jm_newString(char *buff)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode(jm_type_string)) == NULL)
		return NULL;

	node->value = strdup(buff);

	return node;
}

jm_object_t *jm_newLiteral(char *buff)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode(jm_type_literal)) == NULL)
		return NULL;

	node->value = strdup(buff);

	return node;
}

jm_object_t *jm_locate(jm_object_t * node, char *key)
{

	if (!key)
		return NULL;

	jm_object_t *next = NULL;
	if (node->type != jm_type_object)
		return NULL;

	next = node->firstChild;

	while (next != NULL && strcmp(next->name, key) != 0)
		next = next->nextSibling;

	return next;
}

int jm_moveInto(jm_object_t * node, char *key, jm_object_t * child)
{
	jm_object_t *next = NULL, *last = NULL;

	if (node->type != jm_type_object)
		return 1;

	char *newkey = strdup(key);
	jm_detach(child);
	child->name = newkey;
	child->parent = node;

	if (!(next = node->firstChild)) {
		node->firstChild = child;
		node->lastChild = child;
		child->nextSibling = NULL;
		return 0;
	}

	do {
		if (strcmp(next->name, newkey) == 0)
			break;

		last = next;
		next = next->nextSibling;
	} while (next != NULL);

	if (next != NULL) {	/* Update property */
		if (last != NULL)
			last->nextSibling = child;

		child->nextSibling = next->nextSibling;

		if (next == node->firstChild)
			node->firstChild = child;
		if (next == node->lastChild)
			node->lastChild = child;

		next->parent = NULL;
		jm_free(next);

	} else {		/* Add a new value */
		node->lastChild->nextSibling = child;
		node->lastChild = child;
		child->nextSibling = NULL;
	}

	return 0;
}

int jm_detach(jm_object_t * node)
{
	jm_object_t *parent = NULL, *next = NULL, *last = NULL;
	parent = node->parent;
	if (!parent)
		goto finish;

	/* We use a stupid forward linked list making a trivial operation of
	 * pointing (dest - 1)->nextSibling = src require an iteration. */
	next = parent->firstChild;

	if (node == parent->firstChild) {
		parent->firstChild = node->nextSibling;
		if (node == parent->lastChild) {
			parent->lastChild = NULL;
			goto finish;
		}
	}

	while (next != node && next != NULL) {
		last = next;
		next = next->nextSibling;
	}

	if (last != NULL) {
		last->nextSibling = node->nextSibling;
		if (node == parent->lastChild)
			parent->lastChild = last;
	}

	goto finish;

      finish:
	free(node->name);
	node->name = NULL;
	node->parent = NULL;
	node->nextSibling = NULL;
	return 0;
}

int jm_moveOver(jm_object_t * dest, jm_object_t * src)
{
	jm_object_t *parent = dest->parent, *next = NULL, *last = NULL;
	if (!parent)
		return 1;

	jm_detach(src);

	src->name = dest->name != NULL ? strdup(dest->name) : NULL;
	src->parent = dest->parent;

	next = parent->firstChild;

	while (next != dest && next != NULL) {
		last = next;
		next = next->nextSibling;
	}
	if (last != NULL) {
		last->nextSibling = src;
	}

	src->nextSibling = dest->nextSibling;

	if (dest == parent->lastChild)
		parent->lastChild = src;
	if (dest == parent->firstChild)
		parent->firstChild = src;

	/* Unset dest->parent to avoid screwups in jm_free -> jm_detach */
	dest->parent = NULL;
	jm_free(dest);

	return 0;
}

int jm_arrayPush(jm_object_t * node, jm_object_t * child)
{
	/* An array is a linked list */
	if (node->type != jm_type_array)
		return 1;

	/* Already in array */
	if (child->parent == node)
		return 2;

	jm_detach(child);

	child->parent = node;
	child->nextSibling = NULL;

	if (node->lastChild != NULL) {
		node->lastChild->nextSibling = child;
		node->lastChild = child;
	} else {
		node->firstChild = child;
		node->lastChild = child;
	}

	return 0;
}

int jm_arrayInsertAt(jm_object_t * node, int index, jm_object_t * child)
{
	/* An array is a linked list */
	if (node->type != jm_type_array)
		return 1;

	jm_detach(child);

	child->parent = node;

	if (!node->firstChild) {
		node->firstChild = child;
		node->lastChild = child;
	} else {
		jm_object_t *next = NULL, *last = NULL;
		next = node->firstChild;
		int i;

		for (i = 0; i < index; i++) {
			if (!next) {
				return 3;
			}
			last = next;
			next = next->nextSibling;
		}

		if (!last) {	/* index = 0 */
			node->firstChild = child;
			child->nextSibling = next;

		} else {
			last->nextSibling = child;
			child->nextSibling = next;
		}
	}

	return 0;
}

int jm_moveIntoId(jm_object_t * ids, char *id, jm_object_t * node)
{
	/* Create a wrapper node as each node can only have one parent and one
	 * name.  The wrapper is an array, while the type of the inner node
	 * needs to be jm_type_unknown to avoid freeing its child nodes */
	int ret;
	jm_object_t *wrp = NULL, *wrpinner;

	if ((wrp = jm_newArray()) == NULL)
		return 1;

	if ((wrpinner = jm_newNode(jm_type_lid)) == NULL) {
		free(wrp);
		return 1;
	}

	if ((ret = jm_arrayPush(wrp, wrpinner))) {
		jm_free(wrp);
		jm_free(wrpinner);
		return ret;
	}

	if ((ret = jm_moveInto(ids, id, wrp))) {
		jm_free(wrp);
		return ret;
	}

	/* Set the node as parent to inner wrapper, this makes it so we
	 * don't mistakenly free it.  This limits us to a sub set of jm_*
	 * methods, but it should be sufficient. */
	wrpinner->firstChild = node;

	return 0;
}

jm_object_t *jm_locateId(jm_object_t * ids, char *id)
{
	jm_object_t *wrp = NULL, *wrpinner = NULL;
	if ((wrp = jm_locate(ids, id)) == NULL)
		return NULL;

	wrpinner = wrp->firstChild;

	/* The stored node with the specific ID is always the inner wrappers
	 * parent */
	return wrpinner->firstChild;
}

static void jm_free_(jm_object_t * node)
{
	jm_object_t *next = NULL, *last = NULL;
	struct jm_indicators_s *indicators = NULL;

	free(node->name);

	switch (node->type) {
	case jm_type_lid:
		break;
	case jm_type_object:
		indicators = node->indicators;

		if (indicators->extends)
			jm_free_(indicators->extends);
		if (indicators->inserter)
			jm_free_(indicators->inserter);
		if (indicators->move)
			jm_free_(indicators->move);
		if (indicators->value)
			jm_free_(indicators->value);
		if (indicators->matchers)
			jm_free_(indicators->matchers);

		free(indicators);

		/* Fallthough */
	case jm_type_array:
		next = node->firstChild;
		while (next) {
			last = next;
			next = last->nextSibling;
			jm_free_(last);
		}
		break;
	case jm_type_string:
	case jm_type_literal:
	case jm_type_lappend:
	case jm_type_lprepend:
	case jm_type_linsert:
	case jm_type_ldelete:
	case jm_type_loverride:
	case jm_type_lmatch:
		free(node->value);
		break;
	}

	if (node->filename)
		free(node->filename);

	free(node);
}

void jm_free(jm_object_t * node)
{
	if (!node)
		return;

	jm_detach(node);

	return jm_free_(node);
}
