#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "autostr.h"
#include "jsontpl.h"
#include "verify.h"

typedef enum {
    STATE_LITERAL,
    STATE_READ_NAME,
    STATE_READ_FILTER,
    STATE_READ_ARRAY_NAME,
    STATE_READ_ARRAY_VALUE,
    STATE_READ_OBJECT_NAME,
    STATE_READ_OBJECT_KEY,
    STATE_READ_OBJECT_VALUE,
} jsontpl_state;

typedef enum {
    SCOPE_FILE,
    SCOPE_ARRAY,
    SCOPE_OBJECT,
} jsontpl_scope;

#ifdef JSONTPL_MAIN
static char *jsontpl_scope_terminator[3] = {
    "EOF",
    "{[/]}",
    "{{/}}",
};
#endif

static char *jsontpl_alloc(const char *str)
{
    char *rtn = malloc(strlen(str) + 1);
    strcpy(rtn, str);
    return rtn;
}

static int jsontpl_check_name(json_t *context, autostr *name, autostr *full_name)
{
    verify(name->len, "empty name");
    verify(json_object_get(context, name->ptr) != NULL,
        "unknown name %s", full_name->ptr);
    return 0;
}

static int jsontpl_encode(json_t *value, autostr *full_name, char **output)
{
    char *encoded;
    
    switch (json_typeof(value)) {
        
        case JSON_NULL:
            encoded = jsontpl_alloc("null");
            break;
        
        case JSON_FALSE:
            encoded = jsontpl_alloc("false");
            break;
        
        case JSON_TRUE:
            encoded = jsontpl_alloc("true");
            break;
        
        case JSON_STRING:
            encoded = jsontpl_alloc(json_string_value(value));
            break;
        
        case JSON_INTEGER:
        case JSON_REAL:
            encoded = json_dumps(value, JSON_ENCODE_ANY);
            break;
        
        default:
            verify_fail("cannot stringify %s", full_name->ptr);
    }
    
    *output = encoded;
    return 0;
}

static int jsontpl_clone(json_t *json, json_t **clone)
{
    json_error_t error;
    char *clone_str = json_dumps(json, 0);
    *clone = json_loads(clone_str, 0, &error);
    verify(*clone != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    free(clone_str);
    return 0;
}

static int jsontpl_filter(json_t *value, const char *filter, json_t **out)
{
    if (!strcmp(filter, "upper") || !strcmp(filter, "lower")) {
        char upper = (filter[0] == 'u');
        verify(json_is_string(value), JSONTPL_FILTER_ERROR, filter, "string");
        autostr *transformed = autostr_apply(autostr_append(autostr_new(),
            json_string_value(value)), upper ? toupper : tolower);
        *out = json_string(autostr_value(transformed));
        autostr_free(&transformed);
    
    } else if (!strcmp(filter, "count")) {
        verify(json_is_array(value) || json_is_object(value),
            JSONTPL_FILTER_ERROR, filter, "array or object");
        *out = json_integer(json_is_array(value) ?
            json_array_size(value) : json_object_size(value));
    
    } else {
        verify_fail("unknown filter '%s'", filter);
    }
    
    return 0;
}

/* Convenience macro. */
#define verify_seq(p, ...) verify(p, "invalid sequence: " __VA_ARGS__)  

static int jsontpl_parse(char *template,
                     size_t offset,
                     json_t *context,
                     jsontpl_scope scope,
                     char **output,
                     size_t *last_offset)
{
    char c, next, nextnext;
    jsontpl_state state = STATE_LITERAL;
    json_t *name_context = NULL;
    autostr *buffer = autostr_new(),
            *full_name = NULL,
            *name = NULL,
            *filter = NULL,
            *key = NULL,
            *value = NULL;
    /* Used in the STATE_READ_*NAME block. */
    const char *braces;
    jsontpl_scope terminator;
    jsontpl_state next_state;
    char *encoded;
    /* Used in the STATE_READ_*_VALUE blocks. */
    size_t nested_offset;
    size_t nested_index;
    const char *nested_key;
    json_t *nested_value;
    json_t *nested_context;
    char *nested_output;
    
    while ((c = template[offset]) != '\0') {
        next = template[offset + 1];
        nextnext = next ? template[offset + 2] : '\0';
        /* Useful for debugging:
           verify_log_("%c", c); */
        
        switch (state) {
            
            case STATE_LITERAL:
                /* The 'default' state. Template characters are outputted
                   directly unless it's a curly brace or backslash. */
                switch (c) {
                    
                    case '{':
                        /* Curly brace encountered during literal text: about
                           to read a name, possibly of an array or object. */
                        autostr_recycle(&name);
                        autostr_recycle(&full_name);
                        verify_call(jsontpl_clone(context, &name_context));
                        switch (next) {
                            case '[':
                                state = STATE_READ_ARRAY_NAME;
                                offset++;
                                break;
                            case '{':
                                state = STATE_READ_OBJECT_NAME;
                                offset++;
                                break;
                            default:
                                state = STATE_READ_NAME;
                        }
                        break;
                    
                    case '}':
                        /* Closing curly braces could be treated literally, but
                           this would make certain errors difficult to debug,
                           so require them to be escaped. */
                        verify_fail("unmatched }");
                    
                    case '\\':
                        /* Backslash encountered during literal text: if it
                           precedes a curly brace or another backslash, print
                           that character literally and skip this backslash.
                           Otherwise, print this backslash literally. */
                        switch (next) {                        
                            case '{':
                            case '}':
                            case '\\':
                                autostr_push(buffer, next);
                                offset++;
                                break;
                            default:
                                autostr_push(buffer, c);
                        }
                        break;
                    
                    default:
                        /* Literal character: write to the output buffer. */
                        autostr_push(buffer, c);
                }
                break;
            
            case STATE_READ_NAME:
            case STATE_READ_ARRAY_NAME:
            case STATE_READ_OBJECT_NAME:
                /* All name states have some behavior in common, so they're
                   merged into one block of code. */
                switch (state) {
                    case STATE_READ_ARRAY_NAME:
                        braces = "[]";
                        terminator = SCOPE_ARRAY;
                        next_state = STATE_READ_ARRAY_VALUE;
                        break;
                    case STATE_READ_OBJECT_NAME:
                        braces = "{}";
                        terminator = SCOPE_OBJECT;
                        next_state = STATE_READ_OBJECT_KEY;
                        break;
                    default:
                        break;
                }
                switch (c) {
                    
                    case '/':
                        /* Forward slashes are only legal for array and object
                           names, and they must be the only character in the
                           name. Such a sequence indicates the end of an array
                           or object block. */
                        verify_seq(state != STATE_READ_NAME, "{.../");
                        verify_seq(!autostr_len(name), "{%c.../", braces[0]);
                        verify_seq(next == braces[1], "{%c/%c", braces[0], next);
                        verify_seq(nextnext == '}', "{%c/%c%c", braces[0], braces[1], nextnext);
                        *last_offset = offset + 2;
                        json_decref(name_context);
                        jsontpl_exit_scope(terminator);
                    
                    case '}':
                        /* Closing curly braces cannot follow array or object
                           names, which must first be followed by a value name
                           or key -> value pair, respectively. Otherwise, the
                           brace terminates the name and returns to literal
                           text parsing. */
                        verify_seq(state == STATE_READ_NAME, "{%c...}", braces[0]);
                        autostr_trim(name);
                        autostr_trim(full_name);
                        verify_call(jsontpl_check_name(name_context, name, full_name));
                        verify_call(jsontpl_encode(json_object_get(name_context, name->ptr), full_name, &encoded));
                        autostr_append(buffer, encoded);
                        free(encoded);
                        json_decref(name_context);
                        state = STATE_LITERAL;
                        break;
                    
                    case ':':
                        /* Colons terminate array and object names and mark the
                           start of the array value name or object key name. */
                        verify_seq(state != STATE_READ_NAME, "{...:");
                        autostr_trim(name);
                        /* Only one of the key or value needs to be recycled,
                           depending on the state, but doing both won't hurt. */
                        autostr_recycle(&key);
                        autostr_recycle(&value);
                        state = next_state;
                        break;
                    
                    case '.':
                        /* Periods indicate object subscripting. The name up to
                           the period must be an object in the context. */
                        autostr_trim(name);
                        verify_call(jsontpl_check_name(name_context, name, full_name));
                        json_t *obj = json_object_get(name_context, name->ptr);
                        verify(json_is_object(obj), "%s: not an object", full_name->ptr);
                        json_incref(obj);
                        json_decref(name_context);
                        name_context = obj;
                        autostr_recycle(&name);
                        break;
                    
                    case '|':
                        /* Pipes indicate filters. Filters are currently not
                           supported for array / object blocks because there
                           are no filters that return an array or object. */
                        verify_seq(state == STATE_READ_NAME, "{%c...:", braces[0]);
                        autostr_trim(name);
                        autostr_trim(full_name);
                        verify_call(jsontpl_check_name(name_context, name, full_name));
                        autostr_recycle(&filter);
                        state = STATE_READ_FILTER;
                        break;
                    
                    default:
                        /* A character in the name. Names can only contain
                           alphanumeric characters, underscores, spaces,
                           and tabs. */
                        verify_seq(jsontpl_isident(c), "{...%c", c);
                        autostr_push(name, c);
                        autostr_push(full_name, c);
                }
                break;
            
            case STATE_READ_FILTER:
                /* TODO: doc */
                switch (c) {
                
                    case '}':
                        autostr_trim(filter);
                        json_t *obj = json_object_get(name_context, name->ptr);
                        json_t *filtered;
                        verify_call(jsontpl_filter(obj, autostr_value(filter), &filtered));
                        verify_call(jsontpl_encode(obj, full_name, &encoded));
                        autostr_append(buffer, encoded);
                        free(encoded);
                        json_decref(filtered);
                        json_decref(name_context);
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        verify_seq(jsontpl_isident(c), "{...|...%c", c);
                        autostr_push(filter, c);
                }
                break;
            
            case STATE_READ_ARRAY_VALUE:
                /* The array value is the name to which each member of the array
                   will be assigned inside the array block. */
                switch (c) {
                    
                    case ']':
                        /* The sequence is terminated by a ]} pattern. */
                        verify_seq(next == '}', "{[...:...]%c", next);
                        offset += 2;                        
                        autostr_trim(value);
                        verify_seq(value->len, "{[...:]}");
                        
                        /* Set up the environment for the recursively-called
                           parser, which inherits the parent context with the
                           addition of the array value. */
                        nested_offset = offset;
                        json_t *array = json_object_get(name_context, name->ptr);
                        verify(json_is_array(array), "not an array: %s", name->ptr);
                        verify_call(jsontpl_clone(context, &nested_context));
                        verify_call(jsontpl_check_name(name_context, name, full_name));
                        
                        json_array_foreach(array, nested_index, nested_value) {
                            json_object_set(nested_context, value->ptr, nested_value);
                            verify_call(jsontpl_parse(template, nested_offset, nested_context,
                                                       SCOPE_ARRAY, &nested_output, &offset));
                            autostr_append(buffer, nested_output);
                            free(nested_output);
                        }
                        
                        json_decref(name_context);
                        json_decref(nested_context);
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        /* A character in the value. Value names follow the same
                           rules as names. */
                        verify_seq(jsontpl_isident(c), "{[...:...%c", c);
                        autostr_push(value, c);
                }
                break;
                
            case STATE_READ_OBJECT_KEY:
                /* The object key is the name to which each key of the object
                   will be assigned inside the object block. */
                switch (c) {
                    
                    case '-':
                        /* The key is terminated by a -> pattern. */
                        verify_seq(next == '>', "{[...:...-%c", c);
                        autostr_trim(key);
                        autostr_recycle(&value);
                        state = STATE_READ_OBJECT_VALUE;
                        offset++;
                        break;
                    
                    default:
                        /* A character in the key. Key names follow the same
                           rules as names. */
                        verify_seq(jsontpl_isident(c), "{[...:...%c", c);
                        autostr_push(key, c);
                }
                break;
            
            case STATE_READ_OBJECT_VALUE:
                /* The object value is the name to which each value of the object
                   will be assigned inside the object block. */
                switch (c) {
                    
                    case '}':
                        /* The sequence is terminated by a }} pattern. */
                        verify_seq(next == '}', "{{...:...:...}%c", next);
                        offset += 2;
                        autostr_trim(value);
                        verify_seq(value->len, "{{...:...:}}");
                        
                        /* Set up the environment for the recursively-called
                           parser, which inherits the parent context with the
                           addition of the object key and value. */
                        nested_offset = offset;
                        json_t *object = json_object_get(name_context, name->ptr);
                        verify(json_is_object(object), "not an object: %s", name->ptr);
                        verify_call(jsontpl_clone(context, &nested_context));
                        verify_call(jsontpl_check_name(name_context, name, full_name));
                        
                        json_object_foreach(object, nested_key, nested_value) {
                            json_object_set_new(nested_context, key->ptr, json_string(nested_key));
                            json_object_set(nested_context, value->ptr, nested_value);
                            verify_call(jsontpl_parse(template, nested_offset, nested_context,
                                                       SCOPE_OBJECT, &nested_output, &offset));
                            autostr_append(buffer, nested_output);
                            free(nested_output);
                        }
                        
                        json_decref(name_context);
                        json_decref(nested_context);
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        /* A character in the value. Value names follow the same
                           rules as names. */
                        verify_seq(jsontpl_isident(c), "{{...:...->...%c", c);
                        autostr_push(value, c);
                }
                break;
            
            default:
                verify_fail("invalid state");
        }
        
        offset++;
        *last_offset = offset;
    }
    verify(state == STATE_LITERAL, "unexpected EOF");
    jsontpl_exit_scope(SCOPE_FILE);
}

#undef verify_seq

int jsontpl(char *json_filename, char *template_filename, char **output)
{
    // Get JSON data
    json_error_t error;
    json_t *root = json_load_file(json_filename, JSON_REJECT_DUPLICATES, &error);
    verify(root != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify(json_is_object(root), "root is not an object");
    
    // Open template file
    FILE *template_file = fopen(template_filename, "rb");
    verify(template_file != NULL, "%s: no such file", template_filename);
    // Get file size
    fseek(template_file, 0, SEEK_END);
    long lSize = ftell(template_file);
    rewind(template_file);
    // Read template into memory
    char *template = malloc(lSize + 1);
    template[lSize] = '\0';
    size_t filesize = fread(template, 1, lSize, template_file);
    verify(filesize == lSize, "%s: read error", template_filename);
    // Cleanup
    fclose(template_file);
    
    // Parse the template
    size_t last_offset = (size_t)-1;
    verify_call(jsontpl_parse(template, 0, root, SCOPE_FILE, output, &last_offset));
    free(template);
    json_decref(root);
    
    return 0;
}

#ifdef JSONTPL_MAIN
int main(int argc, char *argv[])
{
    // Check arg count
    if (argc != 4) {
        char *progname = "jsontpl";
        if (argc) progname = argv[0];
        fprintf(stderr, "USAGE: %s json-file template-file output-file\n", progname);
        return 1;
    }
    
    char *output = NULL;
    int rtn = jsontpl(argv[1], argv[2], &output);
    
    if (output) {
        FILE *outfile = fopen(argv[3], "wb");
        fwrite(output, 1, strlen(output), outfile);
        fclose(outfile);
        free(output);
    }
    
    return rtn;
}
#endif