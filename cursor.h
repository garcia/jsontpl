#ifndef CURSOR_H
#define CURSOR_H

typedef struct {
    const char *buffer;
    size_t offset;
    size_t line;
    size_t column;
} cursor_t;

cursor_t *cursor(const char *buffer);
size_t cursor_offset(cursor_t *c);
size_t cursor_line(cursor_t *c);
size_t cursor_column(cursor_t *c);
char cursor_peek(cursor_t *c);
char cursor_read(cursor_t *c);
void cursor_move(cursor_t *c, size_t chars);

#endif // CURSOR_H