#ifndef _JM_H
#define _JM_H 1
#include <stdio.h>
#define JM_PRETTY 1

#define PROGRAM_NAME "json_merger"
#ifndef PROGRAM_VERSION
#define PROGRAM_VERSION "UNKNOWN VERSION"
#endif

struct jm_object_s;
typedef struct jm_object_s jm_object_t;

enum jm_type_e {
	jm_type_unknown,
	jm_type_object,
	jm_type_array,
	jm_type_string,
	jm_type_literal		/* null, numbers, booleans and JavaScript */
};

/* Indicators set on objects */
struct jm_indicators_s {
	jm_object_t *extends;	/* Array */
	/* Used for arrays */
	jm_object_t *append;	/* Boolean */
	jm_object_t *prepend;	/* Boolean */
	jm_object_t *insert;	/* Number */
	/* Works the same as @insert, but used with @match */
	jm_object_t *move;	/* Number */
	/* Remap value of the object to what's in @value (Usefull for appending
	 * / prepending to arrays */
	jm_object_t *value;	/* Anything */
	jm_object_t *override;	/* Boolean / Array of keys */
	jm_object_t *delete;	/* Boolean / Array of keys */
	jm_object_t *match;	/* String */
};

struct jm_object_s {
	enum jm_type_e type;
	char *name;		/* Name when stored in object */
	struct jm_indicators_s *indicators;	/* @indicator values */
	char *value;		/* Primitive value */
	char *filename;		/* File path */
	jm_object_t *firstChild;
	jm_object_t *lastChild;
	jm_object_t *nextSibling;
	jm_object_t *parent;
};

/* Functions */
jm_object_t *jm_newObject(void);
jm_object_t *jm_newArray(void);
jm_object_t *jm_newString(char *buff);
jm_object_t *jm_newLiteral(char *buff);
jm_object_t *jm_locate(jm_object_t * node, char *key);
jm_object_t *jm_query(jm_object_t * node, char *selector);

int jm_moveInto(jm_object_t * node, char *key, jm_object_t * child);
int jm_moveOver(jm_object_t * dest, jm_object_t * src);
int jm_arrayPush(jm_object_t * node, jm_object_t * child);
int jm_arrayInsertAt(jm_object_t * node, int index, jm_object_t * child);

int jm_detach(jm_object_t * node);
void jm_free(jm_object_t * node);
jm_object_t *jm_parseFile(char *file);

jm_object_t *jm_parse(char *source);
int jm_serialize(char *outfh, jm_object_t * node, int flags);
jm_object_t *jm_merge(jm_object_t * dest, jm_object_t * vars);

#endif
