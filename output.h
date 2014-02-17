#ifndef OUTPUT_H
#define OUTPUT_H

#define OUTPUT_MACROS 1

typedef enum {
    OUTPUT_STR,
    OUTPUT_FILE,
} output_type;

typedef struct {
    output_type type;
    char write;
    autostr_t *str;
    FILE *file;
} output_t;

// Constructors / destructors:

output_t *output_str(autostr_t *str);
output_t *output_file(FILE *file);
void output_free(output_t **o);

// Getters / setters:

#if OUTPUT_MACROS
#define output_get_type(o) (o->type)
#define output_get_write(o) (o->write)
#define output_get_str(o) (o->str)
#define output_get_file(o) (o->file)
#define output_set_write(o, w) (o->write = (w))
#else // OUTPUT_MACROS
output_type output_get_type(output_t *o);
FILE *output_get_file(output_t *o);
autostr_t *output_get_str(output_t *o);
char output_get_write(output_t *o);
void output_set_write(output_t *o, char write);
#endif // OUTPUT_MACROS

// Other methods:

void output_push(output_t *o, char ch);
void output_append(output_t *o, const char *str);

#endif // CURSOR_H