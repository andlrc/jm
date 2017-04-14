#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jx.h"

char *text;			/* Used for error messages, contains original source to parse */
char *ch;			/* The current character */
size_t lineno;			/* Contains numbers of lines */
size_t llineno;			/* Contains offset for last lineno is */

static void white(void);
static jx_object_t *object(void);
static jx_object_t *array(void);
static char *string(void);
static char *number(void);
static char *literal(void);
static jx_object_t *value(void);

static void err(char expected)
{
	fprintf(stderr, "Expected '%c' instead of '%c' at %zu:%zu\n",
		expected, *ch, (size_t) lineno, (ch - text - llineno));
	exit(1);
}

static enum jx_indicators_e indicator(char *key)
{
	if (*key++ != '@')
		return jx_indicator_unknown;

	switch (*key++) {
	case 'e':
		return strcmp(key, "xtends") == 0
		    ? jx_indicator_extends : jx_indicator_unknown;
		break;
	case 'a':
		return strcmp(key, "ppend") == 0
		    ? jx_indicator_append : jx_indicator_unknown;
		break;
	case 'p':
		return strcmp(key, "repend") == 0
		    ? jx_indicator_prepend : jx_indicator_unknown;
		break;
	case 'i':
		return strcmp(key, "nsert") == 0
		    ? jx_indicator_insert : jx_indicator_unknown;
		break;
	case 'm':
		return strcmp(key, "ove") == 0
		    ? jx_indicator_move : strcmp(key, "atch") == 0
		    ? jx_indicator_match : jx_indicator_unknown;
		break;
	case 'v':
		return strcmp(key, "alue") == 0
		    ? jx_indicator_value : jx_indicator_unknown;
		break;
	case 'o':
		return strcmp(key, "verride") == 0
		    ? jx_indicator_override : jx_indicator_unknown;
		break;
	case 'd':
		return strcmp(key, "elete") == 0
		    ? jx_indicator_delete : jx_indicator_unknown;
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
		goto finish;
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
			fprintf(stderr, "json_merger: @ID isn't supported\n");
			break;
		default:
			jx_moveInto(object, key, val);
		}

		free(key);
		white();

		if (*ch == '}') {
			ch++;
			goto finish;
		}

		if (*ch != ',') {
			jx_free(object);
			err(',');
			return NULL;
		}
		ch++;

		white();
	}

	goto err;

      err:
	jx_free(object);
	return NULL;
      finish:
	return object;
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
		goto finish;
	}

	while (ch != '\0') {
		jx_arrayPush(array, value());
		white();
		if (*ch == ']') {
			ch++;
			goto finish;
		}

		if (*ch != ',') {
			jx_free(array);
			err(',');
			return NULL;
		}
		ch++;
		white();
	}

	goto err;

      err:
	jx_free(array);
	return NULL;
      finish:
	return array;
}

static char *string(void)
{
	if (*ch != '"') {
		err('"');
		return NULL;
	}
	ch++;

	size_t buff_size = 256;
	size_t i = 0;
	char *buff = NULL;

	if ((buff = malloc(buff_size)) == NULL)
		return NULL;

	while (*ch != '\0') {
		switch (*ch) {
		case '"':
			ch++;
			goto finish;
			break;
		case '\\':
			ch++;
			switch (*ch) {
			case '"':
				/* Fallthough */
			case '\\':
				buff[i++] = *ch;
				break;
			case 'b':
				buff[i++] = '\b';
				break;
			case 'f':
				buff[i++] = '\f';
				break;
			case 'n':
				buff[i++] = '\n';
				break;
			case 'r':
				buff[i++] = '\r';
				break;
			case 't':
				buff[i++] = '\t';
				break;
			default:
				goto err;
			}
			break;
		default:
			buff[i++] = *ch;
		}
		ch++;

		if (i == buff_size) {
			buff_size *= 2;
			if (realloc(buff, buff_size) == NULL)
				goto err;
		}
	}

	goto err;

      err:
	free(buff);
	return NULL;
      finish:
	buff[i] = '\0';
	return buff;
}

static char *number(void)
{
	char *string = NULL;
	int i = 0;

	if ((string = malloc(256)) == NULL)
		return 0;

	if (*ch == '-')
		string[i++] = *ch;

	while (*ch >= '0' && *ch <= '9') {
		string[i++] = *ch;
		ch++;
	}

	if (*ch == '.') {
		string[i++] = *ch;
		while (*(++ch) != '\0' && *ch >= '0' && *ch <= '9') {
			string[i++] = *ch;
		}
	}

	if (*ch == 'e' || *ch == 'E') {
		string[i++] = *ch;
		ch++;
		if (*ch == '-' || *ch == '+')
			string[i++] = *ch;

		while (*ch >= '0' && *ch <= '9') {
			string[i++] = *ch;
			ch++;
		}
	}

	string[i] = '\0';
	return string;
}

static int maybe(char *seek)
{
	for (size_t i = 0; seek[i] != '\0'; i++) {
		if (ch[i] != seek[i]) {
			return 1;
		}
	}

	return 0;
}

static char *literal(void)
{
	/* Code buffer */
	char *buff = NULL;
	size_t buff_size = 256, i = 0;
	int brackets[2] = { 0 };

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

	while (*ch != '\0') {
		/* Slurp everything until a closing {curly, square} bracket
		 * or comma. Also count pairs of opening and closing brackets */
		if (brackets[0] == 0 && brackets[1] == 0) {
			if (*ch == '}' || *ch == ']' || *ch == ',')
				goto finish;
		}

		switch (*ch) {
		case '\n':
			lineno++;
			llineno = (ch - text);
			break;
		case '{':
		case '[':
			brackets[*ch % 3]++;
			break;
		case '}':
		case ']':
			brackets[(*ch - 2) % 3]--;
			break;
		}

		buff[i++] = *ch++;

		if (i == buff_size) {
			buff_size *= 2;
			if (realloc(buff, buff_size) == NULL)
				goto err;
		}
	}

      err:
	free(buff);
	err(brackets[0] ? '}' : ']');
	return NULL;

      finish:
	return buff;
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
