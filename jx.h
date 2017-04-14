#ifndef _JX_H
#define _JX_H 1
#include <stdio.h>
#define JX_PRETTY 1

struct jx_object_s;
typedef struct jx_object_s jx_object_t;

enum jx_type_e {
	jx_type_unknown,
	jx_type_object,
	jx_type_array,
	jx_type_string,
	jx_type_literal		/* null, numbers, booleans and JavaScript */
};

enum jx_indicators_e {
	jx_indicator_unknown,
	jx_indicator_extends,
	jx_indicator_append,
	jx_indicator_prepend,
	jx_indicator_insert,
	jx_indicator_move,
	jx_indicator_value,
	jx_indicator_override,
	jx_indicator_delete,
	jx_indicator_match,
	jx_indicators_id
};

/* Indicators set on objects */
struct jx_indicators_s {
	jx_object_t *extends;	/* Array */
	/* Used for arrays */
	jx_object_t *append;	/* Boolean */
	jx_object_t *prepend;	/* Boolean */
	jx_object_t *insert;	/* Number */
	/* Works the same as @insert, but used with @match */
	jx_object_t *move;	/* Number */
	/* Remap value of the object to what's in @value (Usefull for appending
	 * / prepending to arrays */
	jx_object_t *value;	/* Anything */
	jx_object_t *override;	/* Boolean / Array of keys */
	jx_object_t *delete;	/* Boolean / Array of keys */
	jx_object_t *match;	/* String */
};

struct jx_object_s {
	enum jx_type_e type;
	char *name;		/* Name when stored in object */
	struct jx_indicators_s *indicators;	/* @indicator values */
	char *value;		/* Primitive value */
	char *filename;		/* File path */
	jx_object_t *firstChild;
	jx_object_t *lastChild;
	jx_object_t *nextSibling;
	jx_object_t *parent;
};

/* Functions */
jx_object_t *jx_newObject(void);
jx_object_t *jx_newArray(void);
jx_object_t *jx_newString(char *buff);
jx_object_t *jx_newLiteral(char *buff);
jx_object_t *jx_locate(jx_object_t * node, char *key);

int jx_moveInto(jx_object_t * node, char *key, jx_object_t * child);
int jx_arrayPush(jx_object_t * node, jx_object_t * child);

void jx_free(jx_object_t * node);
jx_object_t *jx_parseFile(char *file);

jx_object_t *jx_parse(char *source);
int jx_serialize(char *outfh, jx_object_t * node, int flags);
int jx_merge(jx_object_t * dest, jx_object_t * vars);

#endif
