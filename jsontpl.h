#ifndef JSONTPL_H
#define JSONTPL_H

#include <stdio.h>

/**
 * Parse a JSON string and template string and assign `output` to a newly
 * allocated string pointer.
 */
int jsontpl_string(char *json, char *template, char **output);

/**
 * Parse a JSON file and template file and assign `output` to a newly allocated
 * string pointer.
 */
int jsontpl_file(char *json_filename, char *template_filename, FILE *output);

#endif