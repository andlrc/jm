#include <string.h>
#include <stdlib.h>
#include "jx.h"

struct jx_queryRule_s {
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
	int not;		/* Boolean */
	int isString;		/* Boolean */
	char *key;
	char *value;
	enum valueType_e {
		rule_valueType_unknown,
		rule_valueType_number,
		rule_valueType_string
	} valueType;
	struct jx_queryRule_s *next;
};

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
	node->filename = NULL;

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

	if (!key)
		return NULL;

	jx_object_t *next = NULL;
	if (node->type != jx_type_object)
		return NULL;

	next = node->firstChild;

	while (next != NULL && strcmp(next->name, key) != 0)
		next = next->nextSibling;

	return next;
}

static int isNumeric(char *str)
{
	if (*str == '-')
		str++;

	while (*str >= '0' && *str <= '9')
		str++;

	if (*str == '.') {
		str++;
		while (*str >= '0' && *str <= '9')
			str++;
	}

	if (*str == 'e' || *str == 'E') {
		str++;

		if (*str == '-' || *str == '+')
			str++;

		while (*str >= '0' && *str <= '9')
			str++;
	}

	return *str == '\0';
}

static jx_object_t *query(jx_object_t * node,
			  struct jx_queryRule_s *rule)
{
	size_t rlen, nlen;
	char *val;

	if (!node || !rule)
		return NULL;

	if (node->type == jx_type_array) {
		jx_object_t *next = NULL;
		next = node->firstChild;

		while (next != NULL) {
			if ((node = query(next, rule)) != NULL)
				return node;
			next = next->nextSibling;
		}
	}

	while (rule != NULL) {
		if (rule->key != NULL) {
			node = jx_locate(node, rule->key);
			if (!node)
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
			if (node->type != jx_type_string
			    && node->type != jx_type_literal)
				return NULL;
			if (strcmp(node->value, rule->value) != 0)
				return NULL;
			break;
		case rule_type_beginsWith:
			if (node->type != jx_type_string
				&& node->type != jx_type_literal)
				return NULL;
			rlen = strlen(rule->value);
			if (strncmp(node->value, rule->value, rlen) != 0)
				return NULL;
			break;
		case rule_type_endsWith:
			if (node->type != jx_type_string
				&& node->type != jx_type_literal)
				return NULL;
			rlen = strlen(rule->value);
			nlen = strlen(node->value);
			if (rlen > nlen)
				return NULL;
			if (strncmp(node->value + nlen - rlen,
				rule->value, rlen) != 0)
				return NULL;
			break;
		case rule_type_contains:
			if (node->type != jx_type_string
				&& node->type != jx_type_literal)
				return NULL;
			val = node->value;
			while (*val != '\0') {
				if (strcmp(val, rule->value) == 0)
					break;
				val++;
			}
			if (*val == '\0')
				return NULL;
			break;
		case rule_type_value:
			if (node->type != jx_type_string
				&& node->type != jx_type_literal)
				return NULL;

			if (strcmp(node->value, rule->value) != 0)
				return NULL;

		case rule_type_all:	/* noop */
			break;
		case rule_type_id:
			fprintf(stderr,
				"json_merger: ID selector not supported\n");
			return NULL;
			break;
		case rule_type_directory:
			if ((node = query(node, rule->next)))
				return node;
			return NULL;
		}

		rule = rule->next;
	}

	return node;
}

jx_object_t *jx_query(jx_object_t * node, char *selector)
{
	jx_object_t *ret = NULL;	/* Return value */
	char *buff;
	struct jx_queryRule_s *firstRule = NULL, *prevRule = NULL, *rule =
	    NULL;

	if (!node)
		return NULL;

	while (*selector != '\0') {
		if ((rule = malloc(sizeof(struct jx_queryRule_s))) == NULL)
			goto err;
		if (!firstRule)
			firstRule = rule;
		else
			prevRule->next = rule;

		rule->type = rule_type_unknown;
		rule->not = 0;
		rule->isString = 0;
		rule->key = NULL;
		rule->value = NULL;
		rule->valueType = rule_valueType_unknown;
		rule->next = NULL;

		switch (*selector++) {
		case '!':
			rule->not = 1;
			break;
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
			if (*selector == '"') {
				selector++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL)
				goto err;
			rule->key = buff;

			while (*selector != '\0'
			       && (rule->isString ? *selector !=
				   '"' : *selector != '='
				   && *selector != ']')) {
				/* Character is escaped */
				if (*selector == '\\')
					selector++;

				*buff++ = *selector++;
			}

			*buff = '\0';

			if (rule->isString && *selector != '"')
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
			if (*selector == '"') {
				selector++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL)
				goto err;
			rule->value = buff;

			while (*selector != '\0'
			       && (rule->isString ? *selector !=
				   '"' : *selector != '='
				   && *selector != ']')) {
				/* Character is escaped */
				if (*selector == '\\')
					selector++;

				*buff++ = *selector++;
			}

			*buff = '\0';

			if (rule->isString && *selector != '"')
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

		if (!rule->isString && rule->value != NULL
		    && isNumeric(rule->value))
			rule->valueType = rule_valueType_number;
		else
			rule->valueType = rule_valueType_string;
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

int jx_moveInto(jx_object_t * node, char *key, jx_object_t * child)
{
	jx_object_t *next = NULL, *last = NULL;

	if (node->type != jx_type_object)
		return 1;

	char *newkey = strdup(key);
	jx_detach(child);
	child->name = newkey;
	child->parent = node;

	next = node->firstChild;

	if (!next) {
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
		jx_free(next);

	} else {		/* Add a new value */
		node->lastChild->nextSibling = child;
		node->lastChild = child;
		child->nextSibling = NULL;
	}

	return 0;
}

int jx_detach(jx_object_t * node)
{
	jx_object_t *parent = NULL, *next = NULL, *last = NULL;
	parent = node->parent;
	if (!parent)
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
	free(node->name);
	node->name = NULL;
	node->parent = NULL;
	node->nextSibling = NULL;
	return 0;
}

int jx_moveOver(jx_object_t * dest, jx_object_t * src)
{
	jx_object_t *parent = dest->parent, *next = NULL, *last = NULL;
	if (!parent)
		return 1;

	jx_detach(src);

	src->name = dest->name != NULL ? strdup(dest->name) : NULL;

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

	/* Unset dest->parent to avoid screwups in jx_free -> jx_detach */
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

	jx_detach(child);

	child->parent = node;

	if (!node->firstChild) {
		node->firstChild = child;
		node->lastChild = child;
	} else {
		jx_object_t *next = NULL, *last = NULL;
		next = node->firstChild;

		for (int i = 0; i < index; i++) {
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

static void jx_free2(jx_object_t *node)
{
	jx_object_t *next = NULL, *last = NULL;
	struct jx_indicators_s *indicators = NULL;

	free(node->name);

	switch (node->type) {
	case jx_type_unknown:
		fprintf(stderr,
			"json_merger: cannot free UNKNOWN object\n");
		break;
	case jx_type_object:
		indicators = node->indicators;

		if (indicators->extends)
			jx_free2(indicators->extends);
		if (indicators->append)
			jx_free2(indicators->append);
		if (indicators->prepend)
			jx_free2(indicators->prepend);
		if (indicators->insert)
			jx_free2(indicators->insert);
		if (indicators->move)
			jx_free2(indicators->move);
		if (indicators->value)
			jx_free2(indicators->value);
		if (indicators->override)
			jx_free2(indicators->override);
		if (indicators->delete)
			jx_free2(indicators->delete);
		if (indicators->match)
			jx_free2(indicators->match);

		free(indicators);

		/* Fallthough */
	case jx_type_array:
		next = node->firstChild;
		while (next) {
			last = next;
			next = last->nextSibling;
			jx_free2(last);
		}
		break;
	case jx_type_string:
	case jx_type_literal:
		free(node->value);
		break;
	}

	if (node->filename)
		free(node->filename);

	free(node);
}

void jx_free(jx_object_t * node)
{
	if (!node)
		return;

	jx_detach(node);

	return jx_free2(node);
}

jx_object_t *jx_parseFile(char *file)
{
	FILE *fh = strcmp(file, "-") == 0 ? stdin : fopen(file, "rb");
	char *buff = NULL, *source = NULL;
	size_t buff_size = 256;
	char ch;

	if (!fh) {
		fprintf(stderr, "json_merger: cannot access '%s'\n", file);
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
