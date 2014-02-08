#ifndef JSONTPL_H
#define JSONTPL_H

#define JSONTPL_JSON_ERROR "invalid JSON: %s (line %d, column %d)\n"
#define JSONTPL_FILTER_ERROR "filter '%s' requires %s"

typedef enum {
    SCOPE_FILE,
    SCOPE_BLOCK,
    SCOPE_DISCARD,
} jsontpl_scope;

int jsontpl_string(char *json, char *template, char **output);
int jsontpl_file(char *json_filename, char *template_filename, char **output);

#endif