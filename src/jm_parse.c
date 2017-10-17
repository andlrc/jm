#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jm.h"

#define INDICATOR_MARKER '@'

struct jm_parser {
	char *source;		/* Used for error messages, contains original
				   source to parse */
	char *ch;		/* The current character */
	size_t lineno;		/* Contains numbers of lines */
	size_t llineno;		/* Contains offset for where last lineno is */
	jm_object_t *ids;	/* Contains storage for ID's */
};

enum jm_indicators_e {
	jm_indicator_unknown,
	jm_indicator_extends,
	jm_indicator_append,
	jm_indicator_prepend,
	jm_indicator_insert,
	jm_indicator_move,
	jm_indicator_value,
	jm_indicator_override,
	jm_indicator_delete,
	jm_indicator_match,
	jm_indicator_id,
	jm_indicator_comment
};

static char *string(struct jm_parser *p);
static jm_object_t *value(struct jm_parser *p);

static void err(struct jm_parser *p, char ex)
{
	char expected[4] = "EOF",
	     got[4] = "EOF";

	if (ex != '\0')
		sprintf(expected, "'%c'", ex);
	if (*p->ch != '\0')
		sprintf(got, "'%c'", *p->ch);

	fprintf(stderr, "%s: expected %s instead of %s at %zu:%zu\n",
		PROGRAM_NAME, expected, got,
		p->lineno, p->ch - p->source - p->llineno);
}

static int add_inserter(jm_object_t * object, jm_object_t * in,
			enum jm_indicators_e type)
{
	jm_object_t *inserter = NULL;

	switch (in->type) {
	case jm_type_literal:	/* @append: true, @prepend: true, @insert: NUM */
		switch (type) {
		case jm_indicator_append:
			inserter = jm_newNode(jm_type_lappend);
			break;
		case jm_indicator_prepend:
			inserter = jm_newNode(jm_type_lprepend);
			break;
		case jm_indicator_insert:
			inserter = jm_newNode(jm_type_linsert);
			inserter->value = strdup(in->value);
			break;
		default:
			return 1;
			break;
		}
		object->indicators->inserter = inserter;
		break;
	default:
		return 1;
		break;
	}

	return 0;
}

static int add_matcher_(jm_object_t * matchers, char *selector,
			enum jm_indicators_e type)
{
	jm_object_t *matcher = NULL;
	switch (type) {
	case jm_indicator_delete:
		matcher = jm_newNode(jm_type_ldelete);
		matcher->value = strdup(selector);
		return jm_arrayPush(matchers, matcher);
		break;
	case jm_indicator_override:
		matcher = jm_newNode(jm_type_loverride);
		matcher->value = strdup(selector);
		return jm_arrayPush(matchers, matcher);
		break;
	case jm_indicator_match:	/* Match have to come first as it can be
					   combined with the others */
		matcher = jm_newNode(jm_type_lmatch);
		matcher->value = strdup(selector);
		return jm_arrayInsertAt(matchers, 0, matcher);
		break;
	default:
		return 1;
		break;
	}
}

static int add_matcher(jm_object_t * object, jm_object_t * in,
		       enum jm_indicators_e type)
{
	jm_object_t *matchers = NULL, *next = NULL;
	matchers = object->indicators->matchers;
	if (!matchers) {
		matchers = jm_newArray();
		object->indicators->matchers = matchers;
	}

	switch (in->type) {
	case jm_type_literal:	/* @delete: true, @override: true */
		if (strcmp(in->value, "true") == 0) {
			/* Match itself */
			return add_matcher_(matchers, ".", type);
		}
		break;
	case jm_type_array:	/* @delete: ["selector"], @override: ["selector"] */
		next = in->firstChild;
		while (next) {
			int ret = 0;
			if ((ret =
			     add_matcher_(matchers, next->value, type)))
				return ret;
			next = next->nextSibling;
		}
		break;
	case jm_type_string:	/* @delete: "selector", @override: "selector",
				   @match: "selector" */
		return add_matcher_(matchers, in->value, type);
		break;
	default:
		return 1;
		break;
	}

	return 0;
}

static enum jm_indicators_e indicator(char *key)
{
	if (*key++ == INDICATOR_MARKER) {
		switch (*key++) {
		case 'a':	/* @append */
			return jm_indicator_append;
			break;
		case 'c':	/* @comment */
			return jm_indicator_comment;
		case 'd':	/* @delete */
			return jm_indicator_delete;
			break;
		case 'e':	/* @extends */
			return jm_indicator_extends;
			break;
		case 'i':	/* @id | @insert */
			switch (*key++) {
			case 'd':	/* @id */
				return jm_indicator_id;
				break;
			case 'n':	/* @insert */
				return jm_indicator_insert;
				break;
			}
			break;
		case 'm':	/* @match | @move */
			switch (*key++) {
			case 'a':	/* @match */
				return jm_indicator_match;
				break;
			case 'o':	/* @move */
				return jm_indicator_move;
				break;
			}
			break;
		case 'o':	/* @override */
			return jm_indicator_override;
			break;
		case 'p':	/* @prepend */
			return jm_indicator_prepend;
			break;
		case 'v':	/* @value */
			return jm_indicator_value;
			break;
		}
	}
	return jm_indicator_unknown;
}

static void white(struct jm_parser *p)
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
		jm_object_t *val = NULL;
		char *key = NULL;
		enum jm_indicators_e type;

		if (!(key = string(p))) {
			jm_free(object);
			return NULL;
		}

		white(p);
		if (*p->ch != ':') {
			free(key);
			jm_free(object);
			err(p, ':');
			return NULL;
		}
		p->ch++;
		if (!(val = value(p))) {
			free(key);
			jm_free(object);
			return NULL;
		}

		type = indicator(key);
		switch (type) {
		case jm_indicator_extends:
			object->indicators->extends = val;
			break;
		case jm_indicator_append:	/* At most one inserter per object */
		case jm_indicator_prepend:
		case jm_indicator_insert:
			if (add_inserter(object, val, type)) {
				jm_free(val);
				jm_free(object);
				free(key);
				return NULL;
			}
			jm_free(val);
			break;
		case jm_indicator_move:	/* TODO: Can this be merged with @insert */
			object->indicators->move = val;
			break;
		case jm_indicator_value:
			object->indicators->value = val;
			break;
		case jm_indicator_override:	/* Store all matchers together */
		case jm_indicator_delete:
		case jm_indicator_match:
			if (add_matcher(object, val, type)) {
				jm_free(val);
				jm_free(object);
				free(key);
				return NULL;
			}
			jm_free(val);
			break;
		case jm_indicator_id:
			if (val->type != jm_type_string) {
				fprintf(stderr,
					"%s ID indicator needs to be a string\n",
					PROGRAM_NAME);
				jm_free(object);
				jm_free(val);
				free(key);
				return NULL;
			}
			jm_moveIntoId(p->ids, val->value, object);
			jm_free(val);
			break;
		case jm_indicator_comment:
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

	err(p, '}');
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

	while (*p->ch != '\0') {
		jm_object_t *val = NULL;
		if (!(val = value(p))) {
			jm_free(array);
			return NULL;
		}
		jm_arrayPush(array, val);
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

	err(p, ']');
	jm_free(array);
	return NULL;
}

static char *string(struct jm_parser *p)
{
	size_t buffsize;
	char *buff = NULL, *retbuff = NULL;
	if (*p->ch != '"') {
		err(p, '"');
		return NULL;
	}
	p->ch++;

	buffsize = 256;
	if (!(buff = malloc(buffsize)))
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
			/* fallthrough */
		default:
			*buff++ = *p->ch;
		}
		p->ch++;
		/* At most two bytes is added */
		if (buffsize - 1 <= (size_t) (buff - retbuff)) {
			char *temp;
			buffsize *= 2;
			if (!(temp = realloc(retbuff, buffsize)))
				goto err;
			buff = temp + (buff - retbuff);
			retbuff = temp;
		}
	}

      err:
	err(p, '"');
	free(retbuff);
	return NULL;
}

static char *number(struct jm_parser *p)
{
	char *buff = NULL, *retbuff = NULL;

	if (!(buff = malloc(256)))
		return 0;
	retbuff = buff;
	if (*p->ch == '-')
		*buff++ = *p->ch++;
	while (*p->ch >= '0' && *p->ch <= '9')
		*buff++ = *p->ch++;

	if (*p->ch == '.') {
		*buff++ = *p->ch++;
		while (*p->ch >= '0' && *p->ch <= '9')
			*buff++ = *p->ch++;
	}

	if (*p->ch == 'e' || *p->ch == 'E') {
		*buff++ = *p->ch++;
		if (*p->ch == '-' || *p->ch == '+')
			*buff++ = *p->ch;
		while (*p->ch >= '0' && *p->ch <= '9')
			*buff++ = *p->ch++;
	}

	*buff = '\0';
	return retbuff;
}

static char *literal(struct jm_parser *p)
{

	/* Code buffer */
	char *buff = NULL, *retbuff = NULL;
	size_t buffsize;
	struct brackets_s {
		int cur;
		int sqr;
		int par;
	} brackets = { 0, 0, 0 };
	white(p);
	if (strncmp(p->ch, "true", 4) == 0) {
		p->ch += 4;
		return strdup("true");
	}

	if (strncmp(p->ch, "false", 5) == 0) {
		p->ch += 5;
		return strdup("false");
	}

	if (strncmp(p->ch, "null", 4) == 0) {
		p->ch += 4;
		return strdup("null");
	}

	buffsize = 256;
	if (!(buff = malloc(buffsize)))
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
		if (buffsize <= (size_t) (buff - retbuff)) {
			char *temp;
			buffsize *= 2;
			if (!(temp = realloc(retbuff, buffsize)))
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
	jm_object_t *node = NULL;
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
		if ((str = string(p))) {
			node = jm_newString(str);
			free(str);
		}
		break;
	case '-':
		if ((str = number(p))) {
			node = jm_newLiteral(str);
			free(str);
		}
		break;
	default:
		if ((str = *p->ch >= '0'
		     && *p->ch <= '9' ? number(p) : literal(p))) {
			node = jm_newLiteral(str);
			free(str);
		}
		break;
	}
	return node;
}

jm_object_t *jm_parse(char *source)
{
	jm_object_t *result;
	struct jm_parser *p = NULL;
	if (!(p = malloc(sizeof(struct jm_parser))))
		return NULL;
	p->lineno = 1;
	p->llineno = 0;
	p->ch = source;
	p->source = source;
	p->ids = jm_newObject();
	result = value(p);
	white(p);
	if (result) {
		if (*p->ch != '\0') {
			err(p, '\0');
			free(p);
			jm_free(result);
			return NULL;
		}
		result->ids = p->ids;	/* Used my jm_merge... */
	} else {
		jm_free(p->ids);
	}

	free(p);
	return result;
}

static char *slurpFile(FILE * fh)
{
	char *buff;
	char *bufftmp;
	size_t buffsize = 4096;
	size_t bufflen = 0;
	int c;

	if (!(buff = malloc(buffsize)))
		return NULL;

	while ((c = fgetc(fh)) != EOF) {
		if (bufflen == buffsize) {
			buffsize *= 2;
			if (!(bufftmp = realloc(buff, buffsize))) {
				free(buff);
				return NULL;
			}
			buff = bufftmp;
		}
		buff[bufflen++] = c;
	}

	buff[bufflen++] = '\0';

	return buff;
}

jm_object_t *jm_parseFile(char *file)
{
	FILE *fh = NULL;
	jm_object_t *ret = NULL;
	char *source = NULL;

	if (!(fh = strcmp(file, "-") == 0 ? stdin : fopen(file, "rb"))) {
		fprintf(stderr,
			"%s: cannot access '%s'\n", PROGRAM_NAME, file);
		goto err;
	}

	if (!(source = slurpFile(fh)))
		goto err;

	if (!(ret = jm_parse(source)))
		goto err;

	ret->filename = strdup(file);
      err:
	free(source);
	if (fh && fh != stdin)
		fclose(fh);
	return ret;
}
