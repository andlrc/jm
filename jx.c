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

int jx_arrayPush(jx_object_t * node, jx_object_t * child)
{
	/* An array is a linked list */
	if (node->type != jx_type_array)
		return 1;

	/* Already in array */
	if (child->parent == node)
		return 2;

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

void jx_free(jx_object_t * node)
{
	jx_object_t *next = NULL, *last = NULL;

	free(node->name);
	node->name = NULL;

	if (node->parent != NULL) {
		next = node->parent->firstChild;
		while (next != node) {
			last = next;
			next = last->nextSibling;
		}

		if (last == NULL) {	/* First child */
			node->parent->firstChild = node->nextSibling;
		} else if (node == node->parent->lastChild) {
			node->parent->lastChild = last;
			last->nextSibling = NULL;
		} else {
			last->nextSibling = node->nextSibling;
		}
	}

	switch (node->type) {
	case jx_type_unknown:
		fprintf(stderr,
			"json_merger: cannot free UNKNOWN object\n");
		break;
	case jx_type_object:
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
