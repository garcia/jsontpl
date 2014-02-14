#include <stdlib.h>

#include "autostr.h"
#include "cursor.h"

cursor_t *cursor(const char *buffer)
{
    cursor_t *c = malloc(sizeof(cursor_t));
    c->buffer = buffer;
    c->offset = 0;
    c->line = 1;
    c->column = 1;
    
    return c;
}

size_t cursor_offset(cursor_t *c)
{
    return c->offset;
}

size_t cursor_line(cursor_t *c)
{
    return c->line;
}

size_t cursor_column(cursor_t *c)
{
    return c->column;
}

char cursor_peek(cursor_t *c)
{
    return c->buffer[c->offset];
}

char cursor_read(cursor_t *c)
{
    char ch = cursor_peek(c);
    
    if (ch == '\n') {
        c->line += 1;
        c->column = 0;
    }
    c->offset += 1;
    c->column += 1;
    
    return ch;
}

void cursor_move(cursor_t *c, size_t chars)
{
    while (chars--) {
        cursor_read(c);
    }
}