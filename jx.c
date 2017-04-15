#include <string.h>
#include <stdlib.h>
#include "jx.h"

static jx_object_t *jx_newNode(void)
{
	jx_object_t *node = NULL;

	if ((node = malloc(sizeof(jx_object_t))) == NULL) {
		return NULL;
	}

	node->type = jx_type_unknown;
	node->name = NULL;
	node->indicators = NULL;
	node->value = NULL;
	node->firstChild = NULL;
	node->lastChild = NULL;
	node->nextSibling = NULL;
	node->parent = NULL;

	return node;
}

jx_object_t *jx_newObject(void)
{
	jx_object_t *node = NULL;
	struct jx_indicators_s *indicators = NULL;

	if ((node = jx_newNode()) == NULL)
		return NULL;

	if ((indicators = malloc(sizeof(struct jx_indicators_s))) == NULL) {
		free(node);
		return NULL;
	}

	indicators->extends = NULL;
	indicators->append = NULL;
	indicators->prepend = NULL;
	indicators->insert = NULL;
	indicators->move = NULL;
	indicators->value = NULL;
	indicators->override = NULL;
	indicators->delete = NULL;
	indicators->match = NULL;

	node->indicators = indicators;
	node->type = jx_type_object;

	return node;
}

jx_object_t *jx_newArray(void)
{
	jx_object_t *node = NULL;

	if ((node = jx_newNode()) == NULL)
		return NULL;

	node->type = jx_type_array;

	return node;
}

jx_object_t *jx_newString(char *buff)
{
	jx_object_t *node = NULL;

	if ((node = jx_newNode()) == NULL)
		return NULL;

	node->type = jx_type_string;
	node->value = strdup(buff);

	return node;
}

jx_object_t *jx_newLiteral(char *buff)
{
	jx_object_t *node = NULL;

	if ((node = jx_newNode()) == NULL)
		return NULL;

	node->type = jx_type_literal;
	node->value = strdup(buff);

	return node;
}

jx_object_t *jx_locate(jx_object_t * node, char *key)
{
	jx_object_t *next = NULL;
	if (node->type != jx_type_object)
		return NULL;

	next = node->firstChild;

	while (next != NULL && strcmp(next->name, key) != 0)
		next = next->nextSibling;

	return next;
}

int jx_moveInto(jx_object_t * node, char *key, jx_object_t * child)
{
	jx_object_t *next = NULL, *last = NULL;

	if (node->type != jx_type_object)
		return 1;

	jx_detach(child);

	next = node->firstChild;

	child->name = strdup(key);
	child->parent = node;

	if (next == NULL) {
		node->firstChild = child;
		node->lastChild = child;
		child->nextSibling = NULL;
		return 0;
	}

	do {
		if (strcmp(next->name, key) == 0)
			break;

		last = next;
		next = next->nextSibling;
	} while (next != NULL);

	if (next != NULL) {	/* Update property */
		child->nextSibling = next->nextSibling;

		if (last != NULL)
			last->nextSibling = child;
		if (next == node->firstChild)
			node->firstChild = child;
		if (next == node->lastChild)
			node->lastChild = child;

		next->parent = NULL;
		jx_free(next);

	} else {		/* Add a new value */
		last->nextSibling = child;
		node->lastChild = child;
		child->nextSibling = NULL;
	}

	return 0;
}

int jx_detach(jx_object_t * node)
{
	jx_object_t *parent = NULL, *next = NULL, *last = NULL;
	parent = node->parent;
	if (parent == NULL)
		goto finish;

	/* We use a stupid forward linked list making a trivial operation of
	 * pointing (dest - 1)->nextSibling = src require an iteration. */
	next = parent->firstChild;

	if (node == parent->firstChild)
		parent->firstChild = node->nextSibling;

	while (next != node && next != NULL) {
		last = next;
		next = next->nextSibling;
	}
	if (last != NULL) {
		last->nextSibling = node->nextSibling;
		if (node == parent->lastChild)
			parent->lastChild = last;
	}


      finish:
	node->parent = NULL;
	node->nextSibling = NULL;
	return 0;
}

int jx_moveOver(jx_object_t * dest, jx_object_t * src)
{
	jx_object_t *parent = dest->parent, *next = NULL, *last = NULL;
	if (parent == NULL)
		return 1;

	jx_detach(src);

	next = parent->firstChild;

	while (next != dest && next != NULL) {
		last = next;
		next = next->nextSibling;
	}
	if (last != NULL) {
		last->nextSibling = dest;
	}

	src->nextSibling = dest->nextSibling;

	if (dest == parent->lastChild)
		parent->lastChild = src;
	if (dest == parent->firstChild)
		parent->firstChild = src;

	/* Unset dest->parent to avoid screwups in jx_detach */
	dest->parent = NULL;
	jx_free(dest);

	return 0;
}

int jx_arrayPush(jx_object_t * node, jx_object_t * child)
{
	/* An array is a linked list */
	if (node->type != jx_type_array)
		return 1;

	/* Already in array */
	if (child->parent == node)
		return 2;

	jx_detach(child);

	free(child->name);
	child->name = NULL;
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

int jx_arrayInsertAt(jx_object_t * node, int index, jx_object_t * child)
{
	/* An array is a linked list */
	if (node->type != jx_type_array)
		return 1;

	/* Already in array */
	if (child->parent == node)
		return 2;

	jx_detach(child);

	free(child->name);
	child->name = NULL;
	child->parent = node;

	if (node->firstChild == NULL) {
		node->firstChild = child;
		node->lastChild = child;
	} else {
		jx_object_t *next = NULL, *last = NULL;
		next = node->firstChild;

		for (int i = 0; i < index; i++) {
			if (next == NULL) {
				return 3;
			}
			last = next;
			next = next->nextSibling;
		}

		if (last == NULL) {	/* index = 0 */
			node->firstChild = child;
			child->nextSibling = next;

		} else {
			last->nextSibling = child;
			child->nextSibling = next;
		}
	}

	return 0;
}

void jx_free(jx_object_t * node)
{
	jx_object_t *next = NULL, *last = NULL;

	if (node->name != NULL) {
		free(node->name);
		node->name = NULL;
	}

	/* Remove from parent */
	if (node->parent != NULL) {
		jx_detach(node);
	}

	switch (node->type) {
	case jx_type_unknown:
		fprintf(stderr,
			"json_merger: cannot free UNKNOWN object\n");
		break;
	case jx_type_object:
		/* TODO: free indicators */
		/* Fallthough */
	case jx_type_array:
		next = node->firstChild;
		while (next != NULL) {
			last = next;
			next = last->nextSibling;
			jx_free(last);
		}
		break;
	case jx_type_string:
	case jx_type_literal:
		free(node->value);
		break;
	}

	free(node);
}

jx_object_t *jx_parseFile(char *file)
{
	FILE *fh = strcmp(file, "-") == 0 ? stdin : fopen(file, "rb");
	char *source = NULL;
	size_t buff_size = 256, i = 0;
	char ch;

	if (fh == NULL) {
		fprintf(stderr, "json_merger: cannot access '%s'\n", file);
		goto err;
	}
	if ((source = malloc(buff_size)) == NULL)
		goto err;
	while ((ch = getc(fh)) != EOF) {
		source[i++] = ch;

		if (i == buff_size) {
			char *temp;
			buff_size *= 2;
			if ((temp = realloc(source, buff_size)) == NULL)
				goto err;
			source = temp;
		};
	}

	source[i] = '\0';

	jx_object_t *node = jx_parse(source);
	free(source);
	if (fh != stdin)
		fclose(fh);
	node->filename = strdup(file);
	return node;

      err:
	free(source);
	if (fh != NULL && fh != stdin)
		fclose(fh);
	return NULL;
}
