#include <string.h>
#include <stdlib.h>
#include "jm.h"

struct jm_parser_s {
	char *ch;		/* Current location */
	char *selector;		/* Input selector, used for error messages */
};

struct jm_qrule_s {
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
	struct jm_qrule_s *next;
};

static int isword(char c)
{
	return (c != '\0') && ((c >= 'a' && c <= 'z')
			       || (c >= 'A' && c <= 'Z')
			       || (c >= '0' && c <= '9')
			       || (c == '_'));
}

static void err(struct jm_parser_s *p, char ex)
{
	char got[4] = "EOF";

	if (*p->ch != '\0')
		sprintf(got, "'%c'", *p->ch);
	fprintf(stderr,
		"%s: expected '%c' instead of %s in '%s'\n",
		PROGRAM_NAME, ex, got, p->selector);
}

static jm_object_t *query(jm_object_t * node, struct jm_qrule_s *rule,
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

static void jm_freeqr(struct jm_qrule_s *rule)
{
	struct jm_qrule_s *last = NULL;
	while (rule != NULL) {
		free(rule->key);
		free(rule->value);
		last = rule;
		rule = rule->next;
		free(last);
	}
}

static struct jm_qrule_s *parse(char *selector)
{
	char *buff;
	struct jm_qrule_s *frule = NULL, *prevRule = NULL, *rule = NULL;
	struct jm_parser_s *p = NULL;

	if ((p = malloc(sizeof(struct jm_parser_s))) == NULL)
		return NULL;

	p->selector = selector;
	p->ch = selector;

	while (*p->ch != '\0') {
		if ((rule = malloc(sizeof(struct jm_qrule_s))) == NULL) {
			jm_freeqr(frule);
			return NULL;
		}
		if (!frule)
			frule = rule;
		else
			prevRule->next = rule;

		rule->type = rule_type_unknown;
		rule->isString = 0;
		rule->key = NULL;
		rule->value = NULL;
		rule->next = NULL;

		switch (*p->ch++) {
		case '[':
			if (strncmp(p->ch, "@value=", 7) == 0) {
				rule->type = rule_type_value;
				p->ch += 7;
				goto attrval;
			}
			if (strncmp(p->ch, "@id=", 4) == 0) {
				rule->type = rule_type_id;
				p->ch += 4;
				goto attrval;
			}
			if (*p->ch == '\'') {
				p->ch++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL) {
				jm_freeqr(frule);
				return NULL;
			}
			rule->key = buff;

			while (*p->ch != '\0'
			       && (rule->isString ? *p->ch !=
				   '\'' : isword(*p->ch))) {
				/* Character is escaped */
				if (*p->ch == '\\')
					p->ch++;

				*buff++ = *p->ch++;
			}

			*buff = '\0';

			if (rule->isString && *p->ch++ != '\'') {
				jm_freeqr(frule);
				p->ch--;	/* For error message */
				err(p, '\'');
				return NULL;
			}

			/* isString was set before for parsing the key, but it
			 * really represent the value */
			rule->isString = 0;

			switch (*p->ch++) {
			case ']':
				rule->type = rule_type_haveAttr;
				goto cont;
				break;
			case '=':
				rule->type = rule_type_equal;
				break;
			case '^':
				if (*p->ch != '=') {
					jm_freeqr(frule);
					err(p, '=');
					return NULL;
				}
				p->ch++;
				rule->type = rule_type_beginsWith;
				break;
			case '$':
				if (*p->ch != '=') {
					jm_freeqr(frule);
					err(p, '=');
					return NULL;
				}
				p->ch++;
				rule->type = rule_type_endsWith;
				break;
			case '*':
				if (*p->ch != '=') {
					jm_freeqr(frule);
					err(p, '=');
					return NULL;
				}
				p->ch++;
				rule->type = rule_type_contains;
				break;
			default:
				p->ch--;	/* For error message */
				jm_freeqr(frule);
				err(p, ']');
				return NULL;
				break;
			}

		      attrval:

			/* Find value */
			if (*p->ch == '\'') {
				p->ch++;
				rule->isString = 1;
			}

			if ((buff = malloc(256)) == NULL) {
				jm_freeqr(frule);
				return NULL;
			}
			rule->value = buff;

			while (*p->ch != '\0'
			       && (rule->isString ? *p->ch !=
				   '\'' : *p->ch != ']')) {
				/* Character is escaped */
				if (*p->ch == '\\')
					p->ch++;

				*buff++ = *p->ch++;
			}

			*buff = '\0';

			if (rule->isString && *p->ch != '\'') {
				jm_freeqr(frule);
				p->ch--;	/* For error message */
				err(p, '\'');
				return NULL;
			}

			if (*p->ch != ']') {
				err(p, ']');
				jm_freeqr(frule);
				return NULL;
			}
			p->ch++;
		      cont:
			break;
		case '#':
			/* ID match, an ID matches the following regex
			 * [a-zA-Z0-9_] */
			if ((buff = malloc(256)) == NULL) {
				jm_freeqr(frule);
				return NULL;
			}
			rule->value = buff;

			while (isword(*p->ch))
				*buff++ = *p->ch++;
			*buff = '\0';
			rule->type = rule_type_id;
			break;
		case '*':
			rule->type = rule_type_all;
			break;
		default:
			p->ch--;
			/* Directory like query */
			if ((buff = malloc(256)) == NULL) {
				jm_freeqr(frule);
				return NULL;
			}

			if (*p->ch == '/')
				p->ch++;

			rule->key = buff;

			while (*p->ch != '/' && *p->ch != '['
			       && *p->ch != '\0') {
				*buff++ = *p->ch++;
			}
			*buff = '\0';
			rule->type = rule_type_directory;

			if (*p->ch == '/')
				p->ch++;
			break;
		}

		prevRule = rule;

	}

	return frule;
}

jm_object_t *jm_query(jm_object_t * node, char *selector,
		      struct jm_globals_s * globals)
{
	struct jm_qrule_s *rule = NULL;
	jm_object_t *match;

	if (!node)
		return NULL;

	if ((rule = parse(selector)) == NULL)
		return NULL;

	if ((match = query(node, rule, globals)) == NULL) {
		fprintf(stderr, "%s: selector '%s' disn't match\n",
			PROGRAM_NAME, selector);
		jm_freeqr(rule);
		return NULL;
	}

	jm_freeqr(rule);
	return match;
}
