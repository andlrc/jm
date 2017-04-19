#include <string.h>
#include <stdlib.h>
#include "jm.h"

struct jm_queryRule_s {
	enum type_e {
		rule_type_unknown,
		rule_type_haveAttr,
		rule_type_equal,
		rule_type_beginsWith,
		rule_type_endsWith,
		rule_type_contains,
		rule_type_value,
		rule_type_all,
		rule_type_id,
		rule_type_directory
	} type;
	int isString;		/* Boolean */
	char *key;
	char *value;
	struct jm_queryRule_s *next;
};

static jm_object_t *jm_newNode(void)
{
	jm_object_t *node = NULL;

	if ((node = malloc(sizeof(jm_object_t))) == NULL) {
		return NULL;
	}

	node->type = jm_type_unknown;
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

	if ((node = jm_newNode()) == NULL)
		return NULL;

	if ((indicators = malloc(sizeof(struct jm_indicators_s))) == NULL) {
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
	node->type = jm_type_object;

	return node;
}

jm_object_t *jm_newArray(void)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode()) == NULL)
		return NULL;

	node->type = jm_type_array;

	return node;
}

jm_object_t *jm_newString(char *buff)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode()) == NULL)
		return NULL;

	node->type = jm_type_string;
	node->value = strdup(buff);

	return node;
}

jm_object_t *jm_newLiteral(char *buff)
{
	jm_object_t *node = NULL;

	if ((node = jm_newNode()) == NULL)
		return NULL;

	node->type = jm_type_literal;
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

static jm_object_t *query(jm_object_t * node, struct jm_queryRule_s *rule)
{
	size_t rlen, nlen;
	char *val;

	if (!node || !rule)
		return NULL;

	if (node->type == jm_type_array) {
		jm_object_t *next = NULL;
		next = node->firstChild;

		while (next != NULL) {
			if ((node = query(next, rule)) != NULL)
				return node;
			next = next->nextSibling;
		}

		return NULL;
	}

	while (rule != NULL) {
		jm_object_t *childNode = NULL;
		if (rule->key != NULL) {
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
		}
		switch (rule->type) {
		case rule_type_unknown:
			return NULL;
			break;
		case rule_type_haveAttr:
			/* Everything done above */
			break;
		case rule_type_equal:
			if (rule->isString
			    && childNode->type != jm_type_string)
				return NULL;
			if (childNode->type != jm_type_string
			    && childNode->type != jm_type_literal)
				return NULL;
			if (strcmp(childNode->value, rule->value) != 0)
				return NULL;
			break;
		case rule_type_beginsWith:
			if (childNode->type != jm_type_string
			    && childNode->type != jm_type_literal)
				return NULL;
			rlen = strlen(rule->value);
			if (strncmp(childNode->value, rule->value, rlen) !=
			    0)
				return NULL;
			break;
		case rule_type_endsWith:
			if (childNode->type != jm_type_string
			    && childNode->type != jm_type_literal)
				return NULL;
			rlen = strlen(rule->value);
			nlen = strlen(childNode->value);
			if (rlen > nlen)
				return NULL;
			if (strncmp(childNode->value + nlen - rlen,
				    rule->value, rlen) != 0)
				return NULL;
			break;
		case rule_type_contains:
			if (childNode->type != jm_type_string
			    && childNode->type != jm_type_literal)
				return NULL;
			val = childNode->value;
			while (*val != '\0') {
				if (strcmp(val, rule->value) == 0)
					break;
				val++;
			}
			if (*val == '\0')
				return NULL;
			break;
		case rule_type_value:
			if (node->type != jm_type_string
			    && node->type != jm_type_literal)
				return NULL;

			if (strcmp(node->value, rule->value) != 0)
				return NULL;

		case rule_type_all:	/* noop */
			break;
		case rule_type_id:
			fprintf(stderr,
				"%s: ID selector not supported\n",
				PROGRAM_NAME);
			return NULL;
			break;
		case rule_type_directory:
			if ((node = query(childNode, rule->next)))
				return node;
			return NULL;
		}

		rule = rule->next;
	}

	return node;
}

jm_object_t *jm_query(jm_object_t * node, char *selector)
{
	jm_object_t *ret = NULL;	/* Return value */
	char *buff;
	struct jm_queryRule_s *firstRule = NULL, *prevRule = NULL, *rule =
	    NULL;

	if (!node)
		return NULL;

	while (*selector != '\0') {
		if ((rule = malloc(sizeof(struct jm_queryRule_s))) == NULL)
			goto err;
		if (!firstRule)
			firstRule = rule;
		else
			prevRule->next = rule;

		rule->type = rule_type_unknown;
		rule->isString = 0;
		rule->key = NULL;
		rule->value = NULL;
		rule->next = NULL;

		switch (*selector++) {
		case '[':
			if (strncmp(selector, "@value=", 7) == 0) {
				rule->type = rule_type_value;
				selector += 7;
				goto attrval;
			}
			if (strncmp(selector, "@id=", 4) == 0) {
				rule->type = rule_type_id;
				selector += 4;
				goto attrval;
			}
			if (*selector == '\'') {
				selector++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL)
				goto err;
			rule->key = buff;

			while (*selector != '\0'
			       && (rule->isString ? *selector !=
				   '\'' : *selector != '='
				   && *selector != ']')) {
				/* Character is escaped */
				if (*selector == '\\')
					selector++;

				*buff++ = *selector++;
			}

			*buff = '\0';

			if (rule->isString && *selector++ != '\'')
				goto err;

			/* isString was set before for parsing the key, but it
			 * really represent the value */
			rule->isString = 0;

			switch (*selector++) {
			case ']':
				rule->type = rule_type_haveAttr;
				goto cont;
				break;
			case '=':
				rule->type = rule_type_equal;
				break;
			case '^':
				if (*selector++ != '=')
					goto err;
				rule->type = rule_type_beginsWith;
				break;
			case '$':
				if (*selector++ != '=')
					goto err;
				rule->type = rule_type_endsWith;
				break;
			case '*':
				if (*selector++ != '=')
					goto err;
				rule->type = rule_type_contains;
				break;
			default:
				goto err;
				break;
			}

		      attrval:

			/* Find value */
			if (*selector == '\'') {
				selector++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL)
				goto err;
			rule->value = buff;

			while (*selector != '\0'
			       && (rule->isString ? *selector !=
				   '\'' : *selector != ']')) {
				/* Character is escaped */
				if (*selector == '\\')
					selector++;

				*buff++ = *selector++;
			}

			*buff = '\0';

			if (rule->isString && *selector++ != '\'')
				goto err;

			if (*selector++ != ']')
				goto err;
		      cont:
			break;
		case '#':
			/* ID match, an ID matches the following regex
			 * [a-zA-Z0-9_] */
			if ((buff = malloc(256)) == NULL)
				goto err;
			rule->value = buff;

			while ((*selector >= 'a' && *selector <= 'z')
			       || (*selector >= 'A' && *selector <= 'Z')
			       || (*selector >= '0' && *selector <= '9')
			       || *selector == '_')
				*buff++ = *selector;
			*buff = '\0';
			rule->type = rule_type_id;
			break;
		case '*':
			rule->type = rule_type_all;
			break;
		default:
			selector--;
			/* Directory like query */
			if ((buff = malloc(256)) == NULL)
				goto err;

			if (*selector == '/')
				selector++;

			rule->key = buff;

			while (*selector != '/' && *selector != '['
			       && *selector != '\0') {
				*buff++ = *selector++;
			}
			*buff = '\0';
			rule->type = rule_type_directory;

			if (*selector == '/')
				selector++;
			break;
		}

		prevRule = rule;

	}

	ret = query(node, firstRule);

	goto cleanup;
      err:
	ret = NULL;
      cleanup:
	if (firstRule != NULL) {
		rule = firstRule;
		while (rule != NULL) {
			free(rule->key);
			free(rule->value);
			prevRule = rule;
			rule = rule->next;
			free(prevRule);
		}
	}
	return ret;
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

static void jm_free_(jm_object_t * node)
{
	jm_object_t *next = NULL, *last = NULL;
	struct jm_indicators_s *indicators = NULL;

	free(node->name);

	switch (node->type) {
	case jm_type_unknown:
		fprintf(stderr,
			"%s: cannot free UNKNOWN object\n", PROGRAM_NAME);
		break;
	case jm_type_object:
		indicators = node->indicators;

		if (indicators->extends)
			jm_free_(indicators->extends);
		if (indicators->append)
			jm_free_(indicators->append);
		if (indicators->prepend)
			jm_free_(indicators->prepend);
		if (indicators->insert)
			jm_free_(indicators->insert);
		if (indicators->move)
			jm_free_(indicators->move);
		if (indicators->value)
			jm_free_(indicators->value);
		if (indicators->override)
			jm_free_(indicators->override);
		if (indicators->delete)
			jm_free_(indicators->delete);
		if (indicators->match)
			jm_free_(indicators->match);

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

jm_object_t *jm_parseFile(char *file)
{
	FILE *fh = strcmp(file, "-") == 0 ? stdin : fopen(file, "rb");
	char *buff = NULL, *source = NULL;
	size_t buff_size = 256;
	int ch;

	if (!fh) {
		fprintf(stderr, "%s: cannot access '%s'\n", PROGRAM_NAME,
			file);
		goto err;
	}
	if ((buff = malloc(buff_size)) == NULL)
		goto err;
	source = buff;
	while ((ch = getc(fh)) != EOF) {
		*buff++ = ch;

		if (buff_size <= (size_t) (buff - source)) {
			char *temp;
			buff_size *= 2;
			if ((temp = realloc(source, buff_size)) == NULL)
				goto err;
			buff = temp + (buff - source);
			source = temp;
		};
	}

	*buff = '\0';

	jm_object_t *node = jm_parse(source);
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
