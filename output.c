#include <stdio.h>
#include <stdlib.h>

#include "autostr.h"
#include "output.h"

output_t *output_str(autostr_t *str)
{
    output_t *o = malloc(sizeof(output_t));
    o->type = OUTPUT_STR;
    o->write = 1;
    o->str = str;
    o->file = NULL;
    return o;
}

output_t *output_file(FILE *file)
{
    output_t *o = malloc(sizeof(output_t));
    o->type = OUTPUT_FILE;
    o->write = 1;
    o->str = NULL;
    o->file = file;
    return o;
}

void output_free(output_t **o)
{
    if (*o) {
        switch ((*o)->type) {
            case OUTPUT_STR:
                autostr_free(&((*o)->str));
                break;
            case OUTPUT_FILE:
                fclose((*o)->file);
                break;
        }
        free(*o);
        *o = NULL;
    }
}

#if !(OUTPUT_MACROS)
output_type output_get_type(output_t *o) { return o->type; }
char output_get_write(output_t *o) { return o->write; }
autostr_t *output_get_str(output_t *o) { return o->str; }
FILE *output_get_file(output_t *o) { return o->file; }
void output_set_write(output_t *o, char write) { o->write = write; }
#endif // !(OUTPUT_MACROS)

void output_push(output_t *o, char ch)
{
    if (!o->write) return;
    
    switch (o->type) {
        case OUTPUT_STR:
            autostr_push(o->str, ch);
            break;
        case OUTPUT_FILE:
            fputc(ch, o->file);
            break;
    }
}

void output_append(output_t *o, const char *str)
{
    if (!o->write) return;
    
    switch (o->type) {
        case OUTPUT_STR:
            autostr_append(o->str, str);
            break;
        case OUTPUT_FILE:
            fprintf(o->file, "%s", str);
            break;
    }
}