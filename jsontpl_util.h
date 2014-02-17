#ifndef JSONTPL_UTIL_H
#define JSONTPL_UTIL_H

#include <ctype.h>
#include <jansson.h>

#include "autostr.h"
#include "jsontpl.h"
#include "output.h"

#define JSONTPL_JSON_ERROR "invalid JSON: %s (line %d, column %d)\n"
#define JSONTPL_BLOCK_HINT "%s block at line %d, column %d"

#define isident(c) (isalnum(c) || (c) == '_')
#define verify_json_not_null(obj, full_name) \
    verify(obj != NULL, "%s: no such item", full_name->ptr);

typedef enum {
    // Not inside any block. EOF is vaild here.
    SCOPE_FILE = 0x01,
    // Inside a foreach block for a non-empty array or object.
    SCOPE_FOREACH = 0x02,
    // Inside an if block, before the 'else' marker (if any).
    SCOPE_IF = 0x04,
    // Inside an if block, after the 'else' marker.
    SCOPE_ELSE = 0x08,
    // Soft discard: don't check variables or write output. Combined with
    //  SCOPE_FOREACH for empty arrays / objects and SCOPE_IF for false clauses.
    SCOPE_DISCARD = 0x10,
    // Force discard: combined with SCOPE_DISCARD for nested blocks to prevent
    //  nested if-else blocks from writing output.
    SCOPE_FORCE_DISCARD = 0x20,
} jsontpl_scope;

// This actually returns an int, as opposed to "zero or an error code"
int jsontpl_toidentifier(int c);

int valid_name(json_t *context, autostr_t *name, autostr_t *full_name);
int stringify_json(json_t *value, autostr_t *full_name, output_t *output);
int clone_json(json_t *json, json_t **clone);

#endif // JSONTPL_UTIL_H