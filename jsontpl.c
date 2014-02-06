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
static char *scope_terminator[3] = {
    "EOF",
    "{[/]}",
    "{{/}}",
};
#endif

static char *heap_string(const char *str)
{
    char *rtn = malloc(strlen(str) + 1);
    strcpy(rtn, str);
    return rtn;
}

static int valid_name(json_t *context, autostr *name, autostr *full_name)
{
    verify(name->len, "empty name");
    verify(json_object_get(context, name->ptr) != NULL,
        "unknown name %s", full_name->ptr);
    verify_return();
}

static int stringify_json(json_t *value, autostr *full_name, char **out)
{
    switch (json_typeof(value)) {
        
        case JSON_NULL:
            *out = heap_string("null");
            break;
        
        case JSON_FALSE:
            *out = heap_string("false");
            break;
        
        case JSON_TRUE:
            *out = heap_string("true");
            break;
        
        case JSON_STRING:
            *out = heap_string(json_string_value(value));
            break;
        
        case JSON_INTEGER:
        case JSON_REAL:
            *out = json_dumps(value, JSON_ENCODE_ANY);
            break;
        
        default:
            verify_fail("cannot stringify %s", full_name->ptr);
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup free(clone_str)
static int clone_json(json_t *json, json_t **clone)
{
    json_error_t error;
    char *clone_str = json_dumps(json, 0);
    *clone = json_loads(clone_str, 0, &error);
    verify(*clone != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int apply_filter(json_t *value, const char *filter, json_t **out)
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
    
    verify_return();
}

static int valid_exit(jsontpl_scope scope, jsontpl_scope terminator)
{
    verify(scope == terminator, "expected %s but reached %s",
        scope_terminator[scope], scope_terminator[terminator]);
    verify_return();
}

/* Convenience macro.  Note that the first variadic argument gets concatenated
   to the "invalid sequence: " string. */
#define verify_seq(p, ...) verify(p, "invalid sequence: " __VA_ARGS__)

/* Convenience macro.  Blank characters are lazily permitted to allow for 
   surrounding whitespace. */
#define isident(c) (isalnum(c) || isblank(c) || c == '_')

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(name_context);                                              \
    json_decref(nested_context);                                            \
    json_decref(filtered);                                                  \
    autostr_free(&name);                                                    \
    autostr_free(&full_name);                                               \
    autostr_free(&filter);                                                  \
    autostr_free(&key);                                                     \
    autostr_free(&value);                                                   \
    autostr_free(&nested_output);                                           \
} while (0)
static int parse_scope(char *template,
                       size_t offset,
                       json_t *context,
                       jsontpl_scope scope,
                       autostr **output,
                       size_t *last_offset)
{
    char c, next, nextnext;
    jsontpl_state state = STATE_LITERAL;
    
    json_t  *name_context = NULL,
            *nested_context = NULL,
            *nested_value = NULL,
            *filtered = NULL;
    autostr *full_name = NULL,
            *name = NULL,
            *filter = NULL,
            *key = NULL,
            *value = NULL,
            *nested_output = NULL;
    
    /* Used in the STATE_READ_*NAME block. */
    const char *braces;
    jsontpl_scope terminator;
    jsontpl_state next_state;
    char *encoded;
    
    /* Used in the STATE_READ_*_VALUE blocks. */
    size_t nested_offset;
    size_t nested_index;
    const char *nested_key;
    
    *output = autostr_new();
    
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
                        verify_call(clone_json(context, &name_context));
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
                                autostr_push(*output, next);
                                offset++;
                                break;
                            default:
                                autostr_push(*output, c);
                        }
                        break;
                    
                    default:
                        /* Literal character: write to the output buffer. */
                        autostr_push(*output, c);
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
                        verify_call(valid_exit(scope, terminator));
                        verify_return();
                    
                    case '}':
                        /* Closing curly braces cannot follow array or object
                           names, which must first be followed by a value name
                           or key -> value pair, respectively. Otherwise, the
                           brace terminates the name and returns to literal
                           text parsing. */
                        verify_seq(state == STATE_READ_NAME, "{%c...}", braces[0]);
                        autostr_trim(name);
                        autostr_trim(full_name);
                        verify_call(valid_name(name_context, name, full_name));
                        verify_call(stringify_json(json_object_get(name_context, name->ptr), full_name, &encoded));
                        autostr_append(*output, encoded);
                        free(encoded);
                        json_decref(name_context);
                        name_context = NULL;
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
                        verify_call(valid_name(name_context, name, full_name));
                        json_t *obj = json_object_get(name_context, name->ptr);
                        verify(json_is_object(obj), "%s: not an object", full_name->ptr);
                        autostr_push(full_name, '.');
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
                        verify_call(valid_name(name_context, name, full_name));
                        autostr_recycle(&filter);
                        state = STATE_READ_FILTER;
                        break;
                    
                    default:
                        /* A character in the name. Names can only contain
                           alphanumeric characters, underscores, spaces,
                           and tabs. */
                        verify_seq(isident(c), "{...%c", c);
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
                        verify_call(apply_filter(obj, autostr_value(filter), &filtered));
                        verify_call(stringify_json(obj, full_name, &encoded));
                        autostr_append(*output, encoded);
                        free(encoded);
                        json_decref(filtered);
                        filtered = NULL;
                        json_decref(name_context);
                        name_context = NULL;
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        verify_seq(isident(c), "{...|...%c", c);
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
                        verify(json_is_array(array), "not an array: %s", full_name->ptr);
                        verify_call(clone_json(context, &nested_context));
                        verify_call(valid_name(name_context, name, full_name));
                        
                        json_array_foreach(array, nested_index, nested_value) {
                            json_object_set(nested_context, value->ptr, nested_value);
                            verify_call(parse_scope(template, nested_offset, nested_context,
                                                    SCOPE_ARRAY, &nested_output, &offset));
                            autostr_append(*output, autostr_value(nested_output));
                            autostr_free(&nested_output);
                        }
                        
                        json_decref(name_context);
                        name_context = NULL;
                        json_decref(nested_context);
                        nested_context = NULL;
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        /* A character in the value. Value names follow the same
                           rules as names. */
                        verify_seq(isident(c), "{[...:...%c", c);
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
                        verify_seq(isident(c), "{[...:...%c", c);
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
                        verify_call(clone_json(context, &nested_context));
                        verify_call(valid_name(name_context, name, full_name));
                        
                        json_object_foreach(object, nested_key, nested_value) {
                            json_object_set_new(nested_context, key->ptr, json_string(nested_key));
                            json_object_set(nested_context, value->ptr, nested_value);
                            verify_call(parse_scope(template, nested_offset, nested_context,
                                                    SCOPE_OBJECT, &nested_output, &offset));
                            autostr_append(*output, autostr_value(nested_output));
                            autostr_free(&nested_output);
                        }
                        
                        json_decref(name_context);
                        name_context = NULL;
                        json_decref(nested_context);
                        nested_context = NULL;
                        state = STATE_LITERAL;
                        break;
                    
                    default:
                        /* A character in the value. Value names follow the same
                           rules as names. */
                        verify_seq(isident(c), "{{...:...->...%c", c);
                        autostr_push(value, c);
                }
                break;
            
            default:
                verify_fail("invalid state");
        }
        
        offset++;
        *last_offset = offset;
    }
    
    verify_call(valid_exit(scope, SCOPE_FILE));
    verify(state == STATE_LITERAL, "unexpected EOF");
    verify_return();
}

#undef verify_seq
#undef isident

#undef verify_cleanup
#define verify_cleanup
static int valid_root(json_t *root, json_error_t error)
{
    verify(root != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify(json_is_object(root), "root is not an object");
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&output)
static int jsontpl_jansson(json_t *root, char *template, char **out)
{
    autostr *output = NULL;
    size_t last_offset;
    
    if (parse_scope(template, 0, root, SCOPE_FILE, &output, &last_offset)) {
#if VERIFY_LOG
        size_t line_count = 1,
               line_columns = 1,
               offset;
        for (offset = 0; offset < last_offset; offset++) {
            if (template[offset] == '\n') {
                line_count++;
                line_columns = 1;
            } else {
                line_columns++;
            }
        }
        verify_fail("template parsing failed on line %d, column %d", line_count, line_columns);
#else // VERIFY_LOG
        verify_fail("");
#endif // VERIFY_LOG
    }
    
    *out = heap_string(autostr_value(output));
    verify_return();
}


/* Public functions: */


#undef verify_cleanup
#define verify_cleanup json_decref(root)
int jsontpl_string(char *json, char *template, char **out)
{
    json_t *root = NULL;
    json_error_t error;
    
    // Load JSON object from string
    root = json_loads(json, JSON_REJECT_DUPLICATES, &error);
    verify_call(valid_root(root, error));    
    
    verify_call(jsontpl_jansson(root, template, out));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    free(template);                                                         \
    json_decref(root);                                                      \
} while (0)
int jsontpl_file(char *json_filename, char *template_filename, char **out)
{
    char *template = NULL;
    json_t *root = NULL;
    FILE *template_file = NULL;
    json_error_t error;
    long filesize_ftell;
    size_t filesize_fread;
    
    // Load JSON object from file
    root = json_load_file(json_filename, JSON_REJECT_DUPLICATES, &error);
    verify_call(valid_root(root, error));
    
    // Open template file
    template_file = fopen(template_filename, "rb");
    verify(template_file != NULL, "%s: no such file", template_filename);
    
    // Get file size
    fseek(template_file, 0, SEEK_END);
    filesize_ftell = ftell(template_file);
    verify(filesize_ftell != -1L);
    rewind(template_file);
    
    // Read template into memory
    template = malloc(filesize_ftell + 1);
    template[filesize_ftell] = '\0';
    filesize_fread = fread(template, 1, filesize_ftell, template_file);
    verify(filesize_ftell == filesize_fread);
    fclose(template_file);
    
    verify_call(jsontpl_jansson(root, template, out));
    
    verify_return();
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
    int rtn = jsontpl_file(argv[1], argv[2], &output);
    
    if (output) {
        FILE *outfile = fopen(argv[3], "wb");
        fwrite(output, 1, strlen(output), outfile);
        fclose(outfile);
        free(output);
    }
    
    return rtn;
}
#endif