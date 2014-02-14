#include <jansson.h>

#include "autostr.h"
#include "jsontpl_util.h"
#include "verify.h"

int jsontpl_toidentifier(int c)
{
    return isident(c) ? c : '_';
}

#undef verify_cleanup
#define verify_cleanup
int valid_name(json_t *context, autostr_t *name, autostr_t *full_name)
{
    verify(name->len, "empty name");
    verify(json_object_get(context, name->ptr) != NULL,
        "unknown name %s", full_name->ptr);
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
int output_append(autostr_t *output, const char *append, jsontpl_scope scope)
{
    if (!(scope & SCOPE_DISCARD)) {
        autostr_append(output, append);
    }
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
int output_push(autostr_t *output, char c, jsontpl_scope scope)
{
    if (!(scope & SCOPE_DISCARD)) {
        autostr_push(output, c);
    }
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
int stringify_json(
        json_t *value,
        autostr_t *full_name,
        jsontpl_scope scope,
        autostr_t *output)
{
    char *number;
    
    switch (json_typeof(value)) {
        
        case JSON_NULL:
            output_append(output, "null", scope);
            break;
        
        case JSON_FALSE:
            output_append(output, "false", scope);
            break;
        
        case JSON_TRUE:
            output_append(output, "true", scope);
            break;
        
        case JSON_STRING:
            output_append(output, json_string_value(value), scope);
            break;
        
        case JSON_INTEGER:
        case JSON_REAL:
            number = json_dumps(value, JSON_ENCODE_ANY);
            output_append(output, number, scope);
            free(number);
            break;
        
        default:
            verify_fail("cannot stringify %s", full_name->ptr);
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup free(clone_str)
int clone_json(json_t *json, json_t **clone)
{
    json_error_t error;
    char *clone_str = json_dumps(json, 0);
    *clone = json_loads(clone_str, 0, &error);
    verify(*clone != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify_return();
}