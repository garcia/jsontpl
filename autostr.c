#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "autostr.h"

/* Used by autostr_new and autostr_recycle to empty a new or existing string. */
static void autostr_empty(autostr_t *a, char reallocate)
{
    a->size = AUTOSTR_CHUNK;
    a->len = 0;
    a->ptr = reallocate ?
        realloc(a->ptr, AUTOSTR_CHUNK) :
        malloc(AUTOSTR_CHUNK);
    a->ptr[0] = '\0';
}

/* Used by autostr_*trim to reduce the allocated space, if possible. */
static void autostr_shrink(autostr_t *a)
{
    size_t new_size = a->size;
    while (new_size - AUTOSTR_CHUNK > a->len) {
        new_size -= AUTOSTR_CHUNK;
    }
    if (new_size < a->size) {
        a->ptr = realloc(a->ptr, new_size);
        a->size = new_size;
    }
}


/* Public functions: */


autostr_t *autostr()
{
    autostr_t *a = malloc(sizeof(autostr_t));
    autostr_empty(a, 0);
    return a;
}

void autostr_free(autostr_t **a)
{
    if (*a) {
        free((*a)->ptr);
        free(*a);
        *a = NULL;
    }
}

const char *autostr_value(autostr_t *a)
{
    return a->ptr;
}

int autostr_len(autostr_t *a)
{
    return a->len;
}

autostr_t *autostr_recycle(autostr_t **a)
{
    if (*a) {
        autostr_empty(*a, 1);
    } else {
        *a = autostr();
    }
    
    return *a;
}

autostr_t *autostr_append(autostr_t *a, const char *append)
{
    size_t new_len = a->len + strlen(append);
    char reallocate = 0;
    while (a->size <= new_len) {
        a->size += AUTOSTR_CHUNK;
        reallocate = 1;
    }
    if (reallocate) {
        a->ptr = realloc(a->ptr, a->size);
    }
    strcpy(a->ptr + a->len, append);
    a->len = new_len;
    
    return a;
}

autostr_t *autostr_push(autostr_t *a, char push)
{
    if (++a->len == a->size) {
        a->size += AUTOSTR_CHUNK;
        a->ptr = realloc(a->ptr, a->size);
    }
    a->ptr[a->len - 1] = push;
    a->ptr[a->len] = '\0';
    
    return a;
}

autostr_t *autostr_ltrim(autostr_t *a)
{
    size_t i;
    for (i = 0; isspace(a->ptr[i]); i++) {}
    if (i) {
        size_t j = 0;
        while (i <= a->len) {
            a->ptr[j++] = a->ptr[i++];
        }
        a->len = j - 1;
        autostr_shrink(a);
    }
    
    return a;
}

autostr_t *autostr_rtrim(autostr_t *a)
{
    size_t i;
    for (i = a->len; i && isspace(a->ptr[i - 1]); i--) {}
    if (i < a->len) {
        a->len = i;
        a->ptr[i] = '\0';
        autostr_shrink(a);
    }
    
    return a;
}

autostr_t *autostr_trim(autostr_t *a)
{
    return autostr_ltrim(autostr_rtrim(a));
}

int autostr_cmp(autostr_t *a, const char *other)
{
    return strcmp(a->ptr, other);
}

autostr_t *autostr_apply(autostr_t *a, int func(int))
{
    size_t i;
    for (i = 0; i < a->len; i++) {
        a->ptr[i] = func(a->ptr[i]);
    }
    
    return a;
}