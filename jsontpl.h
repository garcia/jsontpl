#ifndef JSONTPL_H
#define JSONTPL_H

#define JSONTPL_JSON_ERROR "invalid JSON: %s (line %d, column %d)\n"
#define JSONTPL_FILTER_ERROR "filter '%s' requires %s"
#define JSONTPL_DUMPS_FLAGS (JSON_ENSURE_ASCII | JSON_ENCODE_ANY)

#define jsontpl_isident(c) (isalnum(c) || isblank(c) || c == '_')

#define jsontpl_exit_scope(terminator) do {                             \
    verify(scope == terminator, "expected %s but reached %s",           \
        jsontpl_scope_terminator[scope],                                \
        jsontpl_scope_terminator[terminator]);                          \
    *output = buffer->ptr;                                              \
    free(buffer);                                                       \
    autostr_free(&name);                                                \
    autostr_free(&full_name);                                           \
    autostr_free(&filter);                                              \
    autostr_free(&key);                                                 \
    autostr_free(&value);                                               \
    return 0;                                                           \
} while (0)

typedef enum {
    JSONTPL_INVALID_FILE,
    JSONTPL_OUT_OF_MEMORY,
    JSONTPL_READ_ERROR,
} jsontpl_return_code;

int jsontpl(char *json_filename, char *template_filename, char **output);

#endif