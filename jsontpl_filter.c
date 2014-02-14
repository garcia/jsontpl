#include <jansson.h>

#include "autostr.h"
#include "jsontpl_filter.h"
#include "jsontpl_util.h"
#include "verify.h"

#undef verify_cleanup
#define verify_cleanup autostr_free(&transformed)
static int transform_string(json_t **obj, int (*func)(int))
{
    autostr *transformed = autostr_apply(autostr_append(autostr_new(),
        json_string_value(*obj)), func);
    *obj = json_string(autostr_value(transformed));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_lower(json_t **obj)
{
    verify_call(transform_string(obj, tolower));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_upper(json_t **obj)
{
    verify_call(transform_string(obj, toupper));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_identifier(json_t **obj)
{
    verify_call(transform_string(obj, jsontpl_toidentifier));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_count(json_t **obj)
{
    *obj = json_integer(json_is_array(*obj) ?
        json_array_size(*obj) : json_object_size(*obj));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&english)
static int filter_english(json_t **obj)
{    
    size_t array_index;
    size_t array_size = json_array_size(*obj);
    autostr *english = autostr_new();
    json_t *array_value; /* borrowed reference */
    
    json_array_foreach(*obj, array_index, array_value) {
        if (array_index && array_index + 1 == array_size) {
            autostr_append(english, "and ");
        }
        verify_call(stringify_json(array_value, NULL, SCOPE_FILE, english));
        if (array_size > 2 && array_index + 1 < array_size) {
            autostr_append(english, ", ");
        }
    }
    
    *obj = json_string(autostr_value(english));
    
    verify_return();
}

static jsontpl_filter_t filters[] = {
    {"lower",       filter_lower,       (json_type []){JSON_STRING, -1}},
    {"upper",       filter_upper,       (json_type []){JSON_STRING, -1}},
    {"identifier",  filter_identifier,  (json_type []){JSON_STRING, -1}},
    {"count",       filter_count,       (json_type []){JSON_ARRAY, JSON_OBJECT}},
    {"english",     filter_english,     (json_type []){JSON_ARRAY, -1}},
    {NULL}
};

#undef verify_cleanup
#define verify_cleanup
int jsontpl_filter(autostr *filter_name, json_t **obj)
{
    json_type type;
    jsontpl_filter_t *filter;
    
    for (filter = &filters[0]; filter->name != NULL; filter++) {
        if (autostr_cmp(filter_name, filter->name) == 0) {
            type = json_typeof(*obj);
            verify(type == filter->types[0] || type == filter->types[1],
                    "invalid type for filter '%s'", autostr_value(filter_name));
            verify_call(filter->func(obj));
            verify_return();
        }
    }
    
    verify_fail("unknown filter '%s'", autostr_value(filter_name));
}