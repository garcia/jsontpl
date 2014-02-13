#ifndef JSONTPL_H
#define JSONTPL_H

#define JSONTPL_JSON_ERROR "invalid JSON: %s (line %d, column %d)\n"
#define JSONTPL_FILTER_ERROR "filter '%s' requires %s"

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

int jsontpl_string(char *json, char *template, char **output);
int jsontpl_file(char *json_filename, char *template_filename, char **output);

#endif