#ifndef JSONTPL_H
#define JSONTPL_H

int jsontpl_string(char *json, char *template, char **output);
int jsontpl_file(char *json_filename, char *template_filename, char **output);

#endif