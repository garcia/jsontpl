#ifndef JSONTPL_FILTER_H
#define JSONTPL_FILTER_H

#include <jansson.h>

#include "autostr.h"

typedef struct {
    char *name;
    int (*func)(json_t **);
    json_type *types;
} jsontpl_filter_t;

int jsontpl_filter(autostr *filter_name, json_t **obj);

#endif // JSONTPL_FILTER_H