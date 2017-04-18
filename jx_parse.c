#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jx.h"

static char *text;		/* Used for error messages, contains original source to parse */
static char *ch;		/* The current character */
static size_t lineno;		/* Contains numbers of lines */
static size_t llineno;		/* Contains offset for where last lineno is */

static void white(void);
static jx_object_t *object(void);
static jx_object_t *array(void);
static char *string(void);
static char *number(void);
static char *literal(void);
static jx_object_t *value(void);

static void err(char expected)
{
	fprintf(stderr,
		"json_merger: Expected '%c' instead of '%c' at %zu:%zu\n",
		expected, *ch, (size_t) lineno, (ch - text - llineno));
	exit(1);
}

static enum jx_indicators_e indicator(char *key)
{
	if (*key++ != '@')
		return jx_indicator_unknown;

	switch (*key++) {
	case 'a':
		return strcmp(key, "ppend") == 0
		    ? jx_indicator_append : jx_indicator_unknown;
		break;
	case 'c':
		return strcmp(key, "omment") == 0
		    ? jx_indicators_comment : jx_indicator_unknown;
	case 'd':
		return strcmp(key, "elete") == 0
		    ? jx_indicator_delete : jx_indicator_unknown;
		break;
	case 'e':
		return strcmp(key, "xtends") == 0
		    ? jx_indicator_extends : jx_indicator_unknown;
		break;
	case 'i':
		return strcmp(key, "nsert") == 0
		    ? jx_indicator_insert : jx_indicator_unknown;
		break;
	case 'm':
		return strcmp(key, "ove") == 0
		    ? jx_indicator_move : strcmp(key, "atch") == 0
		    ? jx_indicator_match : jx_indicator_unknown;
	case 'o':
		return strcmp(key, "verride") == 0
		    ? jx_indicator_override : jx_indicator_unknown;
		break;
	case 'p':
		return strcmp(key, "repend") == 0
		    ? jx_indicator_prepend : jx_indicator_unknown;
		break;
		break;
	case 'v':
		return strcmp(key, "alue") == 0
		    ? jx_indicator_value : jx_indicator_unknown;
		break;
	default:
		return jx_indicator_unknown;
	}
}

static inline void white(void)
{
	/* Skip whitespaces */
	while (*ch != '\0' && *ch <= ' ') {
		if (*ch == '\n') {
			lineno++;
			llineno = (ch - text);
		}
		ch++;
	}
}

static jx_object_t *object(void)
{
	if (*ch != '{') {
		err('{');
		return NULL;
	}
	ch++;
	jx_object_t *object = jx_newObject();
	white();

	/* Empty object */
	if (*ch == '}') {
		ch++;
		return object;
	}

	while (*ch != '\0') {

		char *key = string();

		white();
		if (*ch != ':') {
			jx_free(object);
			free(key);
			err(':');
			return NULL;
		}
		ch++;
		jx_object_t *val = value();

		switch (indicator(key)) {
		case jx_indicator_extends:
			object->indicators->extends = val;
			break;
		case jx_indicator_append:
			object->indicators->append = val;
			break;
		case jx_indicator_prepend:
			object->indicators->prepend = val;
			break;
		case jx_indicator_insert:
			object->indicators->insert = val;
			break;
		case jx_indicator_move:
			object->indicators->move = val;
			break;
		case jx_indicator_value:
			object->indicators->value = val;
			break;
		case jx_indicator_override:
			object->indicators->override = val;
			break;
		case jx_indicator_delete:
			object->indicators->delete = val;
			break;
		case jx_indicator_match:
			object->indicators->match = val;
			break;
		case jx_indicators_id:
			/* TODO: Register node under the ID in `val->value' */
			fprintf(stderr,
				"json_merger: @ID isn't supported\n");
			break;
		case jx_indicators_comment:
			jx_free(val);
			break;
		default:
			jx_moveInto(object, key, val);
		}

		free(key);
		white();

		if (*ch == '}') {
			ch++;
			return object;
		}

		if (*ch != ',') {
			jx_free(object);
			err(',');
			return NULL;
		}
		ch++;

		white();
	}

	/* Error here */
	jx_free(object);
	return NULL;
}

static jx_object_t *array(void)
{
	if (*ch != '[') {
		err('[');
		return NULL;
	}
	ch++;
	jx_object_t *array = jx_newArray();
	white();

	/* Empty array */
	if (*ch == ']') {
		ch++;
		return array;
	}

	while (ch != '\0') {
		jx_arrayPush(array, value());
		white();
		if (*ch == ']') {
			ch++;
			return array;
		}

		if (*ch != ',') {
			jx_free(array);
			err(',');
			return NULL;
		}
		ch++;
		white();
	}

	/* Error here */
	jx_free(array);
	return NULL;
}

static char *string(void)
{
	if (*ch != '"') {
		err('"');
		return NULL;
	}
	ch++;

	size_t buff_size = 256;
	char *buff = NULL, *retbuff = NULL;

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;
	retbuff = buff;

	while (*ch != '\0') {
		switch (*ch) {
		case '"':
			ch++;
			*buff = '\0';
			if (strcmp(retbuff, "Publiceret") == 0)
				return retbuff;
			return retbuff;
			break;
		case '\\':
			*buff++ = *ch++;
			/* Fallthough */
		default:
			*buff++ = *ch;
		}
		ch++;

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

static char *number(void)
{
	char *buff = NULL, *retbuff = NULL;

	if ((buff = malloc(256)) == NULL)
		return 0;
	retbuff = buff;

	if (*ch == '-')
		*buff++ = *ch++;

	while (*ch >= '0' && *ch <= '9') {
		*buff++ = *ch++;
	}

	if (*ch == '.') {
		*buff++ = *ch++;
		while (*ch != '\0' && *ch >= '0' && *ch <= '9') {
			*buff++ = *ch++;
		}
	}

	if (*ch == 'e' || *ch == 'E') {
		*buff++ = *ch++;
		if (*ch == '-' || *ch == '+')
			*buff++ = *ch;

		while (*ch >= '0' && *ch <= '9') {
			*buff++ = *ch++;
		}
	}

	*buff = '\0';
	return retbuff;
}

static int maybe(char *seek)
{
	char *cc = ch;
	while (*seek != '\0')
		if (*cc++ != *seek++)
			return 1;

	return 0;
}

static char *literal(void)
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

	white();

	if (maybe("true") == 0) {
		ch += 4;
		return strdup("true");
	}

	if (maybe("false") == 0) {
		ch += 5;
		return strdup("false");
	}

	if (maybe("null") == 0) {
		ch += 4;
		return strdup("null");
	}

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;
	retbuff = buff;

	while (*ch != '\0') {
		/* Slurp everything until a closing {curly, square} bracket
		 * or comma. Also count pairs of opening and closing brackets */
		if (!brackets.cur && !brackets.sqr && !brackets.par) {
			if (*ch == '}' || *ch == ']' || *ch == ',') {
				*buff = '\0';
				return retbuff;
			}
		}

		switch (*ch) {
		case '\n':
			lineno++;
			llineno = (ch - text);
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

		*buff++ = *ch++;

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
	err(brackets.cur ? '}' : brackets.sqr ? ']' : ')');
	return NULL;
}

static jx_object_t *value(void)
{
	jx_object_t *node;
	char *str;
	white();

	switch (*ch) {
	case '{':
		node = object();
		break;
	case '[':
		node = array();
		break;
	case '"':
		str = string();
		node = jx_newString(str);
		free(str);
		break;
	case '-':
		str = number();
		node = jx_newLiteral(str);
		free(str);
		break;
	default:
		str = *ch >= '0' && *ch <= '9' ? number() : literal();
		node = jx_newLiteral(str);
		free(str);
		break;
	}
	return node;
}

jx_object_t *jx_parse(char *source)
{
	jx_object_t *result;

	lineno = 1;
	llineno = 0;
	ch = source;
	text = source;
	result = value();
	white();
	if (*ch != '\0') {
		fprintf(stderr, "json_merger: syntax errror\n");
		jx_free(result);
		return NULL;
	}

	return result;
}
