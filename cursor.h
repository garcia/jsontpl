#ifndef CURSOR_H
#define CURSOR_H

#define CURSOR_MACROS 1

typedef struct {
    const char *buffer;
    size_t offset;
    size_t line;
    size_t column;
} cursor_t;

// Constructors / destructors:

cursor_t *cursor(const char *buffer);
void cursor_free(cursor_t **c);

// Getters:

#if CURSOR_MACROS
#define cursor_offset(c) ((c)->offset)
#define cursor_line(c) ((c)->line)
#define cursor_column(c) ((c)->column)
#define cursor_peek(c) ((c)->buffer[(c)->offset])
#else // CURSOR_MACROS
size_t cursor_offset(cursor_t *c);
size_t cursor_line(cursor_t *c);
size_t cursor_column(cursor_t *c);
char cursor_peek(cursor_t *c);
#endif // CURSOR_MACROS

// Other methods:

char cursor_read(cursor_t *c);
void cursor_move(cursor_t *c, size_t chars);

#endif // CURSOR_H