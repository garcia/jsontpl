#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "autostr.h"

/* Used by autostr_new and autostr_recycle to empty a new or existing string. */
static void autostr_empty(autostr *instance, char reallocate)
{
    instance->size = AUTOSTR_CHUNK;
    instance->len = 0;
    instance->ptr = reallocate ?
        realloc(instance->ptr, AUTOSTR_CHUNK) :
        malloc(AUTOSTR_CHUNK);
    instance->ptr[0] = '\0';
}

/* Used by autostr_*trim to reduce the allocated space, if possible. */
static void autostr_shrink(autostr *instance)
{
    size_t new_size = instance->size;
    while (new_size - AUTOSTR_CHUNK > instance->len) {
        new_size -= AUTOSTR_CHUNK;
    }
    if (new_size < instance->size) {
        instance->ptr = realloc(instance->ptr, new_size);
        instance->size = new_size;
    }
}

autostr *autostr_new()
{
    autostr *instance = malloc(sizeof(autostr));
    autostr_empty(instance, 0);
    return instance;
}

const char *autostr_value(autostr *instance)
{
    return instance->ptr;
}

int autostr_len(autostr *instance)
{
    return instance->len;
}

void autostr_recycle(autostr **instance)
{
    if (*instance) {
        autostr_empty(*instance, 1);
    } else {
        *instance = autostr_new();
    }
}

void autostr_append(autostr *instance, char *append)
{
    size_t new_len = instance->len + strlen(append);
    char reallocate = 0;
    while (instance->size <= new_len) {
        instance->size += AUTOSTR_CHUNK;
        reallocate = 1;
    }
    if (reallocate) {
        instance->ptr = realloc(instance->ptr, instance->size);
    }
    strcpy(instance->ptr + instance->len, append);
    instance->len = new_len;
}

void autostr_push(autostr *instance, char push)
{
    char append[2] = {push, '\0'};
    autostr_append(instance, append);
}

void autostr_ltrim(autostr *instance)
{
    size_t i;
    for (i = 0; isspace(instance->ptr[i]); i++) {}
    if (i) {
        size_t j = 0;
        while (i <= instance->len) {
            instance->ptr[j++] = instance->ptr[i++];
        }
        instance->len = j - 1;
        autostr_shrink(instance);
    }
}

void autostr_rtrim(autostr *instance)
{
    size_t i;
    for (i = instance->len; i && isspace(instance->ptr[i - 1]); i--) {}
    if (i < instance->len) {
        instance->len = i;
        instance->ptr[i] = '\0';
        autostr_shrink(instance);
    }
}

void autostr_trim(autostr *instance)
{
    autostr_ltrim(instance);
    autostr_rtrim(instance);
}