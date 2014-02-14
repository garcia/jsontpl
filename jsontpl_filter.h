#ifndef JSONTPL_FILTER_H
#define JSONTPL_FILTER_H

#include <jansson.h>

#include "autostr.h"

#define JSON_FILTER_ANY_TYPE (-2)

typedef struct {
    char *name;
    int (*func)(json_t **);
    json_type *types;
} jsontpl_filter_t;

typedef enum {
    LANG_C,
    LANG_PY,
} jsontpl_language;

int jsontpl_filter(autostr_t *filter_name, json_t **obj);

#endif // JSONTPL_FILTER_H