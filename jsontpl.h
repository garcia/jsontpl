#ifndef JSONTPL_H
#define JSONTPL_H

#define JSONTPL_JSON_ERROR "invalid JSON: %s (line %d, column %d)\n"
#define JSONTPL_FILTER_ERROR "filter '%s' requires %s"
#define JSONTPL_DUMPS_FLAGS (JSON_ENSURE_ASCII | JSON_ENCODE_ANY)

int jsontpl_string(char *json, char *template, char **output);
int jsontpl_file(char *json_filename, char *template_filename, char **output);

#endif