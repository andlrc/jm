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

static jm_object_t *query(jm_object_t * node, struct jm_queryRule_s *rule,
			  struct jm_globals_s *globals)
{
	size_t rlen, nlen;
	char *val;

	if (!node || !rule)	/* Can be NULL and a valid pointer which is OK */
		return node;

	if (node->type == jm_type_array) {
		jm_object_t *next = NULL;
		next = node->firstChild;

		while (next != NULL) {
			if ((node = query(next, rule, globals)) != NULL)
				return node;
			next = next->nextSibling;
		}

		return NULL;
	}

	while (rule != NULL) {
		jm_object_t *childNode = NULL;
		switch (rule->type) {
		case rule_type_unknown:
			return NULL;
			break;
		case rule_type_haveAttr:
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
			break;
		case rule_type_equal:
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
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
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
			if (childNode->type != jm_type_string
			    && childNode->type != jm_type_literal)
				return NULL;
			rlen = strlen(rule->value);
			if (strncmp(childNode->value, rule->value, rlen) !=
			    0)
				return NULL;
			break;
		case rule_type_endsWith:
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
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
			childNode = jm_locate(node, rule->key);
			if (!childNode)
				return NULL;
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
			if ((childNode =
			     jm_locate(globals->ids, rule->value)) == NULL)
				return NULL;
			if ((node =
			     query(childNode->firstChild->firstChild,
				   rule->next, globals)))
				return node;
			return NULL;
			break;
		case rule_type_directory:
			if (strcmp(".", rule->key) == 0)	/* noop */
				break;
			if (strcmp("..", rule->key) == 0)	/* Parent */
				childNode = node->parent;
			else
				childNode = jm_locate(node, rule->key);
			if ((node = query(childNode, rule->next, globals)))
				return node;
			return NULL;
		}

		rule = rule->next;
	}

	return node;
}

jm_object_t *jm_query(jm_object_t * node, char *selector,
		      struct jm_globals_s * globals)
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

			while (*selector != '\0' &&
			       ((*selector >= 'a' && *selector <= 'z')
				|| (*selector >= 'A' && *selector <= 'Z')
				|| (*selector >= '0' && *selector <= '9')
				|| *selector == '_'))
				*buff++ = *selector++;
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

	ret = query(node, firstRule, globals);

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
