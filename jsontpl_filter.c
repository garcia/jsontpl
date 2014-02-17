#include <jansson.h>

#include "autostr.h"
#include "jsontpl_filter.h"
#include "jsontpl_util.h"
#include "verify.h"

#undef verify_cleanup
#define verify_cleanup autostr_free(&transformed)
static int transform_string(json_t **obj, int (*func)(int))
{
    autostr_t *transformed = autostr_apply(autostr_append(autostr(),
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
#define verify_cleanup output_free(&english)
static int filter_english(json_t **obj)
{    
    size_t array_index;
    size_t array_size = json_array_size(*obj);
    output_t *english = output_str(autostr());
    json_t *array_value; /* borrowed reference */
    
    json_array_foreach(*obj, array_index, array_value) {
        if (array_index && array_index + 1 == array_size) {
            output_append(english, "and ");
        }
        verify_call(stringify_json(array_value, NULL, english));
        if (array_index + 1 < array_size) {
            if (array_size > 2) {
                output_push(english, ',');
            }
            output_push(english, ' ');
        }
    }
    
    *obj = json_string(autostr_value(output_get_str(english)));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_js(json_t **obj)
{
    char *dump = json_dumps(*obj, JSON_ENCODE_ANY);
    *obj = json_string(dump);
    free(dump);
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&str)
static int filter_lang(jsontpl_language lang, json_t **obj)
{
    autostr_t *str = autostr();
    
    switch (json_typeof(*obj)) {
        
        case JSON_NULL:
            autostr_append(str, (lang == LANG_C) ? "NULL" : "None");
            break;
        
        case JSON_FALSE:
            autostr_append(str, (lang == LANG_C) ? "0" : "False");
            break;
        
        case JSON_TRUE:
            autostr_append(str, (lang == LANG_C) ? "1" : "True");
            break;
        
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_STRING:
            verify_call(filter_js(obj));
            verify_return();
        
        default:
            verify_fail("unknown JSON type");
    }
    
    *obj = json_string(autostr_value(str));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_c(json_t **obj)
{
    verify_call(filter_lang(LANG_C, obj));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int filter_py(json_t **obj)
{
    verify_call(filter_lang(LANG_PY, obj));
    verify_return();
}

static jsontpl_filter_t filters[] = {
    {"lower",       filter_lower,       (json_type []){JSON_STRING, -1}},
    {"upper",       filter_upper,       (json_type []){JSON_STRING, -1}},
    {"identifier",  filter_identifier,  (json_type []){JSON_STRING, -1}},
    {"count",       filter_count,       (json_type []){JSON_ARRAY, JSON_OBJECT, -1}},
    {"english",     filter_english,     (json_type []){JSON_ARRAY, -1}},
    {"js",          filter_js,          (json_type []){JSON_FILTER_ANY_TYPE, -1}},
    {"c",           filter_c,           (json_type []){JSON_NULL, JSON_FALSE, JSON_TRUE,
                                                       JSON_INTEGER, JSON_REAL, JSON_STRING, -1}},
    {"py",          filter_py,          (json_type []){JSON_NULL, JSON_FALSE, JSON_TRUE,
                                                       JSON_INTEGER, JSON_REAL, JSON_STRING, -1}},
    {NULL}
};

#undef verify_cleanup
#define verify_cleanup json_decref(unfiltered_obj)
int jsontpl_filter(autostr_t *filter_name, json_t **obj)
{
    json_type type;
    json_type *type_check;
    char valid_type = 0;
    jsontpl_filter_t *filter;
    json_t *unfiltered_obj = *obj;
    
    for (filter = &filters[0]; filter->name != NULL; filter++) {
        if (autostr_cmp(filter_name, filter->name) == 0) {
            type = json_typeof(*obj);
            for (type_check = filter->types; *type_check != -1; type_check++) {
                if (*type_check == type || *type_check == JSON_FILTER_ANY_TYPE) {
                    valid_type = 1;
                    break;
                }
            }
            verify(valid_type, "invalid type for filter '%s'", autostr_value(filter_name));
            verify_call_hint(filter->func(obj), "filter '%s'", autostr_value(filter_name));
            verify_return();
        }
    }
    
    verify_fail("unknown filter '%s'", autostr_value(filter_name));
}