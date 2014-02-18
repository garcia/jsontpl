#include <jansson.h>

#include "autostr.h"
#include "jsontpl_util.h"
#include "output.h"
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
int stringify_json(json_t *value, autostr_t *full_name, output_t *output)
{
    char *number;
    
    switch (json_typeof(value)) {
        
        case JSON_NULL:
            output_append(output, "null");
            break;
        
        case JSON_FALSE:
            output_append(output, "false");
            break;
        
        case JSON_TRUE:
            output_append(output, "true");
            break;
        
        case JSON_STRING:
            output_append(output, json_string_value(value));
            break;
        
        case JSON_INTEGER:
        case JSON_REAL:
            number = json_dumps(value, JSON_ENCODE_ANY);
            output_append(output, number);
            free(number);
            break;
        
        default:
            verify_fail("cannot stringify %s", full_name->ptr);
    }
    
    verify_return();
}