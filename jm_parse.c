#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jm.h"

struct jm_parser {
	char *source;		/* Used for error messages, contains original source to parse */
	char *ch;		/* The current character */
	size_t lineno;		/* Contains numbers of lines */
	size_t llineno;		/* Contains offset for where last lineno is */
};

static void white(struct jm_parser *p);
static jm_object_t *object(struct jm_parser *p);
static jm_object_t *array(struct jm_parser *p);
static char *string(struct jm_parser *p);
static char *number(struct jm_parser *p);
static char *literal(struct jm_parser *p);
static jm_object_t *value(struct jm_parser *p);

static void err(struct jm_parser *p, char expected)
{
	fprintf(stderr,
		"%s: Expected '%c' instead of '%c' at %zu:%zu\n",
		PROGRAM_NAME, expected, *p->ch, p->lineno,
		(p->ch - p->source - p->llineno));
	exit(1);
}

static int maybe(char *ch, char *seek)
{
	while (*seek != '\0')
		if (*ch++ != *seek++)
			return 1;

	return 0;
}

static enum jm_indicators_e indicator(char *key)
{
	if (*key++ != '@')
		return jm_indicator_unknown;

	switch (*key++) {
	case 'a':
		return strcmp(key, "ppend") == 0
		    ? jm_indicator_append : jm_indicator_unknown;
		break;
	case 'c':
		return strcmp(key, "omment") == 0
		    ? jm_indicators_comment : jm_indicator_unknown;
	case 'd':
		return strcmp(key, "elete") == 0
		    ? jm_indicator_delete : jm_indicator_unknown;
		break;
	case 'e':
		return strcmp(key, "xtends") == 0
		    ? jm_indicator_extends : jm_indicator_unknown;
		break;
	case 'i':
		return strcmp(key, "nsert") == 0
		    ? jm_indicator_insert : jm_indicator_unknown;
		break;
	case 'm':
		return strcmp(key, "ove") == 0
		    ? jm_indicator_move : strcmp(key, "atch") == 0
		    ? jm_indicator_match : jm_indicator_unknown;
	case 'o':
		return strcmp(key, "verride") == 0
		    ? jm_indicator_override : jm_indicator_unknown;
		break;
	case 'p':
		return strcmp(key, "repend") == 0
		    ? jm_indicator_prepend : jm_indicator_unknown;
		break;
		break;
	case 'v':
		return strcmp(key, "alue") == 0
		    ? jm_indicator_value : jm_indicator_unknown;
		break;
	default:
		return jm_indicator_unknown;
	}
}

static inline void white(struct jm_parser *p)
{
	/* Skip whitespaces */
	while (*p->ch != '\0' && *p->ch <= ' ') {
		if (*p->ch == '\n') {
			p->lineno++;
			p->llineno = (p->ch - p->source);
		}
		p->ch++;
	}
}

static jm_object_t *object(struct jm_parser *p)
{
	if (*p->ch != '{') {
		err(p, '{');
		return NULL;
	}
	p->ch++;
	jm_object_t *object = jm_newObject();
	white(p);

	/* Empty object */
	if (*p->ch == '}') {
		p->ch++;
		return object;
	}

	while (*p->ch != '\0') {

		char *key = string(p);

		white(p);
		if (*p->ch != ':') {
			jm_free(object);
			free(key);
			err(p, ':');
			return NULL;
		}
		p->ch++;
		jm_object_t *val = value(p);

		switch (indicator(key)) {
		case jm_indicator_extends:
			object->indicators->extends = val;
			break;
		case jm_indicator_append:
			object->indicators->append = val;
			break;
		case jm_indicator_prepend:
			object->indicators->prepend = val;
			break;
		case jm_indicator_insert:
			object->indicators->insert = val;
			break;
		case jm_indicator_move:
			object->indicators->move = val;
			break;
		case jm_indicator_value:
			object->indicators->value = val;
			break;
		case jm_indicator_override:
			object->indicators->override = val;
			break;
		case jm_indicator_delete:
			object->indicators->delete = val;
			break;
		case jm_indicator_match:
			object->indicators->match = val;
			break;
		case jm_indicators_id:
			/* TODO: Register node under the ID in `val->value' */
			fprintf(stderr,
				"%s: @ID isn't supported\n", PROGRAM_NAME);
			break;
		case jm_indicators_comment:
			jm_free(val);
			break;
		default:
			jm_moveInto(object, key, val);
		}

		free(key);
		white(p);

		if (*p->ch == '}') {
			p->ch++;
			return object;
		}

		if (*p->ch != ',') {
			jm_free(object);
			err(p, ',');
			return NULL;
		}
		p->ch++;

		white(p);
	}

	/* Error here */
	jm_free(object);
	return NULL;
}

static jm_object_t *array(struct jm_parser *p)
{
	if (*p->ch != '[') {
		err(p, '[');
		return NULL;
	}
	p->ch++;
	jm_object_t *array = jm_newArray();
	white(p);

	/* Empty array */
	if (*p->ch == ']') {
		p->ch++;
		return array;
	}

	while (p->ch != '\0') {
		jm_arrayPush(array, value(p));
		white(p);
		if (*p->ch == ']') {
			p->ch++;
			return array;
		}

		if (*p->ch != ',') {
			jm_free(array);
			err(p, ',');
			return NULL;
		}
		p->ch++;
		white(p);
	}

	/* Error here */
	jm_free(array);
	return NULL;
}

static char *string(struct jm_parser *p)
{
	if (*p->ch != '"') {
		err(p, '"');
		return NULL;
	}
	p->ch++;

	size_t buff_size = 256;
	char *buff = NULL, *retbuff = NULL;

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;
	retbuff = buff;

	while (*p->ch != '\0') {
		switch (*p->ch) {
		case '"':
			p->ch++;
			*buff = '\0';
			return retbuff;
			break;
		case '\\':
			*buff++ = *p->ch++;
			/* Fallthough */
		default:
			*buff++ = *p->ch;
		}
		p->ch++;

		/* At most two bytes is added */
		if (buff_size - 1 <= (size_t) (buff - retbuff)) {
			char *temp;
			buff_size *= 2;
			if ((temp = realloc(retbuff, buff_size)) == NULL)
				goto err;
			buff = temp + (buff - retbuff);
			retbuff = temp;
		}
	}

      err:
	free(retbuff);
	return NULL;
}

static char *number(struct jm_parser *p)
{
	char *buff = NULL, *retbuff = NULL;

	if ((buff = malloc(256)) == NULL)
		return 0;
	retbuff = buff;

	if (*p->ch == '-')
		*buff++ = *p->ch++;

	while (*p->ch >= '0' && *p->ch <= '9') {
		*buff++ = *p->ch++;
	}

	if (*p->ch == '.') {
		*buff++ = *p->ch++;
		while (*p->ch != '\0' && *p->ch >= '0' && *p->ch <= '9') {
			*buff++ = *p->ch++;
		}
	}

	if (*p->ch == 'e' || *p->ch == 'E') {
		*buff++ = *p->ch++;
		if (*p->ch == '-' || *p->ch == '+')
			*buff++ = *p->ch;

		while (*p->ch >= '0' && *p->ch <= '9') {
			*buff++ = *p->ch++;
		}
	}

	*buff = '\0';
	return retbuff;
}

static char *literal(struct jm_parser *p)
{
	/* Code buffer */
	char *buff = NULL, *retbuff = NULL;
	size_t buff_size = 256;
	struct brackets_s {
		int cur;
		int sqr;
		int par;
	} brackets = {
	0};

	white(p);

	if (maybe(p->ch, "true") == 0) {
		p->ch += 4;
		return strdup("true");
	}

	if (maybe(p->ch, "false") == 0) {
		p->ch += 5;
		return strdup("false");
	}

	if (maybe(p->ch, "null") == 0) {
		p->ch += 4;
		return strdup("null");
	}

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;
	retbuff = buff;

	while (*p->ch != '\0') {
		/* Slurp everything until a closing {curly, square} bracket
		 * or comma. Also count pairs of opening and closing brackets */
		if (!brackets.cur && !brackets.sqr && !brackets.par) {
			if (*p->ch == '}' || *p->ch == ']'
			    || *p->ch == ',') {
				*buff = '\0';
				return retbuff;
			}
		}

		switch (*p->ch) {
		case '\n':
			p->lineno++;
			p->llineno = p->ch - p->source;
			break;
		case '{':
			brackets.cur++;
			break;
		case '[':
			brackets.sqr++;
			break;
		case '(':
			brackets.par++;
			break;
		case '}':
			brackets.cur--;
			break;
		case ']':
			brackets.sqr--;
			break;
		case ')':
			brackets.par--;
			break;
		}

		*buff++ = *p->ch++;

		if (buff_size <= (size_t) (buff - retbuff)) {
			char *temp;
			buff_size *= 2;
			if ((temp = realloc(retbuff, buff_size)) == NULL)
				goto err;
			buff = temp + (buff - retbuff);
			retbuff = temp;
		}
	}

      err:
	free(retbuff);
	err(p, brackets.cur ? '}' : brackets.sqr ? ']' : ')');
	return NULL;
}

static jm_object_t *value(struct jm_parser *p)
{
	jm_object_t *node;
	char *str;
	white(p);

	switch (*p->ch) {
	case '{':
		node = object(p);
		break;
	case '[':
		node = array(p);
		break;
	case '"':
		str = string(p);
		node = jm_newString(str);
		free(str);
		break;
	case '-':
		str = number(p);
		node = jm_newLiteral(str);
		free(str);
		break;
	default:
		str = *p->ch >= '0'
		    && *p->ch <= '9' ? number(p) : literal(p);
		node = jm_newLiteral(str);
		free(str);
		break;
	}
	return node;
}

jm_object_t *jm_parse(char *source)
{
	jm_object_t *result;

	struct jm_parser *p = NULL;

	if ((p = malloc(sizeof(struct jm_parser))) == NULL)
		return NULL;

	p->lineno = 1;
	p->llineno = 0;
	p->ch = source;
	p->source = source;

	result = value(p);
	white(p);
	if (*p->ch != '\0') {
		fprintf(stderr, "%s: syntax errror\n", PROGRAM_NAME);
		free(p);
		jm_free(result);
		return NULL;
	}

	return result;
}
