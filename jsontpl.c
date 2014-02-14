#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "autostr.h"
#include "jsontpl.h"
#include "jsontpl_filter.h"
#include "jsontpl_util.h"
#include "verify.h"

static int parse_scope(
        char *template,
        json_t *context,
        jsontpl_scope scope,
        size_t *offset,
        autostr *output);

#undef verify_cleanup
#define verify_cleanup
static int discard_blank(char *template, size_t *offset)
{
    char c;
    
    while ((c = template[*offset]) != '\0') {
        if (!isblank(c)) {
            verify_return();
        }
        *offset += 1;
    }
    
    /* EOF is certainly an error here, but let the caller decide on the error
       message, because "EOF encountered while discarding whitespace" wouldn't
       be terribly helpful. */
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int discard_until(char *template, size_t *offset, const char *seq)
{
    char c;
    size_t seq_index = 0;
    
    while ((c = template[*offset]) != '\0') {
        
        if (seq[seq_index] == '\0') {
            verify_return();
        }
        if (c == seq[seq_index]) {
            seq_index++;
        } else {
            seq_index = 0;
        }
        
        *offset += 1;
    }
    
    verify_fail("expected '%s', got EOF", seq);
}

#undef verify_cleanup
#define verify_cleanup
static int parse_seq(char *template, size_t *offset, const char *seq)
{
    char c;
    size_t seq_index = 0;
    
    verify_call(discard_blank(template, offset));
    
    while ((c = template[*offset]) != '\0') {

        verify(c == seq[seq_index], "expected '%s', got '%c'", seq, c);
        seq_index++;
        
        *offset += 1;
        
        if (seq[seq_index] == '\0') {
            verify_return();
        }
    }
    
    verify_fail("expected '%s', got EOF", seq);
}

#undef verify_cleanup
#define verify_cleanup autostr_trim(identifier)
static int parse_identifier(
        char *template,
        size_t *offset,
        autostr *identifier)
{
    char c;
    char seen_ident = 0;
    
    verify_call(discard_blank(template, offset));
    
    while ((c = template[*offset]) != '\0') {
        
        if (isident(c)) {
            seen_ident = 1;
            autostr_push(identifier, c);
        } else {
            verify(seen_ident, "expected an identifier, got '%c'", c);
            verify_call(discard_blank(template, offset));
            verify_return();
        }
        
        *offset += 1;
    }
    
    verify_fail("EOF while reading identifier");
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&filter_name)
int parse_filter(char *template, size_t *offset, json_t **obj)
{
    autostr *filter_name = autostr_new();
    
    verify_call(parse_identifier(template, offset, filter_name));
    verify_call(jsontpl_filter(filter_name, obj));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(name_context);                                              \
    autostr_free(&name);                                                    \
} while (0)
static int parse_name(
        char *template,
        json_t *context,
        size_t *offset,
        autostr *full_name,
        json_t **obj)
{
    char c;
    autostr *name = autostr_new();
    json_t *name_context = NULL;
    
    verify_call(clone_json(context, &name_context));
    verify_call(discard_blank(template, offset));

    while ((c = template[*offset]) != '\0') {
        switch (c) {
            
            case '.':
                /* Periods indicate object subscripting.  The name up to the
                   period must be an object in the context.  That object then
                   becomes the context. */
                verify(autostr_len(name), "empty name component");
                verify_call(valid_name(name_context, name, full_name));
                *obj = json_object_get(name_context, name->ptr);
                verify(json_is_object(*obj), "%s: not an object", full_name->ptr);
                autostr_push(full_name, '.');
                json_incref(*obj);
                json_decref(name_context);
                name_context = *obj;
                autostr_recycle(&name);
                *offset += 1;
                break;
            
            case '|':
                /* Pipes indicate filters. */
                verify_call(valid_name(name_context, name, full_name));
                *obj = json_object_get(name_context, name->ptr);
                json_incref(*obj);
                *offset += 1;
                verify_call(parse_filter(template, offset, obj));
                verify_return();
            
            default:
                if (isident(c)) {
                    verify_call(parse_identifier(template, offset, name));
                    autostr_append(full_name, name->ptr);
                } else {
                    *obj = json_object_get(name_context, name->ptr);
                    json_incref(*obj);
                    verify_return();
                }
        }
        
    }
    
    verify_fail("EOF while reading name");
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(obj);                                                       \
    autostr_free(&full_name);                                               \
} while (0)
static int parse_value(
        char *template,
        json_t *context,
        jsontpl_scope scope,
        size_t *offset,
        autostr *output)
{
    json_t *obj = NULL;
    autostr *full_name = autostr_new();
    
    if (scope & SCOPE_DISCARD) {
        verify_call(discard_until(template, offset, "=}"));
    } else {
        verify_call(parse_name(template, context, offset, full_name, &obj));
        verify_json_not_null(obj, full_name);
        verify_call(parse_seq(template, offset, "=}"));
        verify_call(stringify_json(obj, full_name, scope, output));
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(array_context);                                             \
    autostr_free(&value);                                                   \
} while (0)
static int parse_foreach_array(
        char *template,
        json_t *context,
        json_t *array,
        size_t *offset,
        autostr *output)
{
    size_t starting_offset;
    json_t *array_context = NULL;
    autostr *value = autostr_new();
    // Borrowed references; no need to free during cleanup]
    size_t array_index;
    json_t *array_value;
    
    verify_call(parse_identifier(template, offset, value));
    verify_call(parse_seq(template, offset, "%}"));
    
    if (json_array_size(array) == 0) {
        verify_call(parse_scope(template, context, SCOPE_DISCARD, offset, output));
        verify_return();
    }
    
    starting_offset = *offset;
    verify_call(clone_json(context, &array_context));
    
    json_array_foreach(array, array_index, array_value) {
        json_object_set(array_context, value->ptr, array_value);
        *offset = starting_offset;
        verify_call(parse_scope(template, array_context, SCOPE_FOREACH, offset, output));
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(object_context);                                            \
    autostr_free(&key);                                                     \
    autostr_free(&value);                                                   \
} while (0)
static int parse_foreach_object(
        char *template,
        json_t *context,
        json_t *object,
        size_t *offset,
        autostr *output)
{
    size_t starting_offset;
    json_t *object_context = NULL;
    autostr *key = autostr_new(),
            *value = autostr_new();
    // Borrowed references; no need to free during cleanup
    const char *object_key;
    json_t *object_value;
    
    verify_call(parse_identifier(template, offset, key));
    verify_call(parse_seq(template, offset, "->"));
    verify_call(parse_identifier(template, offset, value));
    verify_call(parse_seq(template, offset, "%}"));
    
    if (json_object_size(object) == 0) {
        verify_call(parse_scope(template, context, SCOPE_DISCARD, offset, output));
        verify_return();
    }
    
    starting_offset = *offset;
    verify_call(clone_json(context, &object_context));
    
    json_object_foreach(object, object_key, object_value) {
        json_object_set_new(object_context, key->ptr, json_string(object_key));
        json_object_set(object_context, value->ptr, object_value);
        *offset = starting_offset;
        verify_call(parse_scope(template, object_context, SCOPE_FOREACH, offset, output));
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(obj);                                                       \
    autostr_free(&full_name);                                               \
} while (0)
static int parse_foreach(
        char *template,
        json_t *context,
        size_t *offset,
        autostr *output)
{
    json_t *obj = NULL;
    autostr *full_name = autostr_new();
    
    verify_call(parse_name(template, context, offset, full_name, &obj));
    verify_json_not_null(obj, full_name);
    verify_call(parse_seq(template, offset, ":"));
    
    if (json_is_array(obj)) {
        verify_call(parse_foreach_array(template, context, obj, offset, output));
    } else if (json_is_object(obj)) {
        verify_call(parse_foreach_object(template, context, obj, offset, output));
    } else {
        verify_fail("foreach block requires an object or array");
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int parse_comment(
        char *template,
        json_t *context,
        size_t *offset,
        autostr *output)
{
    verify_call(parse_seq(template, offset, "%}"));
    verify_call(parse_scope(template, context, SCOPE_DISCARD | SCOPE_FORCE_DISCARD, offset, output));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(obj);                                                       \
    autostr_free(&full_name);                                               \
} while (0)
static int parse_if(
        char *template,
        json_t *context,
        size_t *offset,
        autostr *output)
{
    char discard = 0;
    jsontpl_scope scope = SCOPE_IF;
    json_t *obj = NULL;
    autostr *full_name = autostr_new();
    
    verify_call(parse_name(template, context, offset, full_name, &obj));
    verify_call(parse_seq(template, offset, "%}"));
    
    if (obj == NULL) {
        discard = 1;
    
    } else {
        switch (json_typeof(obj)) {
            
            case JSON_NULL:
            case JSON_FALSE:
                discard = 1;
                break;
            
            case JSON_TRUE:
                break;
            
            case JSON_REAL:
                discard = !json_real_value(obj);
                break;
            
            case JSON_INTEGER:
                discard = !json_integer_value(obj);
                break;
            
            case JSON_STRING:
                discard = !json_string_value(obj)[0];
                break;
            
            case JSON_ARRAY:
                discard = !json_array_size(obj);
                break;
            
            case JSON_OBJECT:
                discard = !json_object_size(obj);
                break;
            
            default:
                verify_fail("internal error: unknown JSON type");
        }
    }
    
    if (discard) scope |= SCOPE_DISCARD;
    
    verify_call(parse_scope(template, context, scope, offset, output));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&block_type)
static int parse_block(
        char *template,
        json_t *context,
        jsontpl_scope scope,
        size_t *offset,
        autostr *output,
        char *end)
{
    *end = 0;
    jsontpl_scope inner_scope;
    size_t offset_here = *offset;
    autostr *block_type = autostr_new();
    
    verify_call(parse_identifier(template, offset, block_type));
    
    if (strcmp(block_type->ptr, "end") == 0) {
        verify_call(parse_seq(template, offset, "%}"));
        *end = 1;
    
    } else if (strcmp(block_type->ptr, "else") == 0) {
        if (scope & SCOPE_FORCE_DISCARD) {
            /* Ignore nested if-else block when discarding input. */
            verify_call(discard_until(template, offset, "%}"));
        } else if (scope & SCOPE_IF) {
            inner_scope = SCOPE_ELSE;
            if (!(scope & SCOPE_DISCARD)) {
                inner_scope |= SCOPE_DISCARD;
            }
            verify_call(parse_seq(template, offset, "%}"));
            verify_call(parse_scope(template, context, inner_scope, offset, output));
            /* Unlike other blocks, else 'steals' the scope from its enclosing
               if block; when it ends, the if block ends as well. */
            *end = 1;
        } else {
            verify_fail("unexpected else marker");
        }
    
    } else if (scope & SCOPE_DISCARD) {
        verify_call(discard_until(template, offset, "%}"));
        inner_scope = SCOPE_DISCARD;
        if (strcmp(block_type->ptr, "if") == 0) {
            /* Normally an else block is allowed to switch the scope from a
               discarding one to non-discarding and vice-versa, but if the
               scope in which the if/else block appears is already discarding,
               the entire block should be silenced. */
            inner_scope |= SCOPE_FORCE_DISCARD;
        }
        verify_call(parse_scope(template, context, inner_scope, offset, output));
            
    } else if (strcmp(block_type->ptr, "foreach") == 0) {
        verify_call_hint(parse_foreach(template, context, offset, output),
            "foreach block at offset %d", offset_here);
        
    } else if (strcmp(block_type->ptr, "if") == 0) {
        verify_call_hint(parse_if(template, context, offset, output),
            "if block at offset %d", offset_here);
        
    } else if (strcmp(block_type->ptr, "comment") == 0) {
        verify_call_hint(parse_comment(template, context, offset, output),
            "comment block at offset %d", offset_here);
        
    } else {
        verify_fail("unknown block type '%s'", block_type->ptr);
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int parse_scope(
        char *template,
        json_t *context,
        jsontpl_scope scope,
        size_t *offset,
        autostr *output)
{
    char c, next;
    char end;
    
    while ((c = template[*offset]) != '\0') {
        next = template[*offset + 1];
        /* Useful for debugging:
           verify_log_("%c", c); */
        
        switch (c) {
            
            case '{':
                /* Curly braces indicate a value or a block. */
                *offset += 1;
                switch (next) {
                    
                    case '=':
                        *offset += 1;
                        verify_call(parse_value(template, context, scope, offset, output));
                        break;
                    
                    case '%':
                        *offset += 1;
                        verify_call(parse_block(template, context, scope, offset, output, &end));
                        if (end) {
                            verify(scope != SCOPE_FILE, "unmatched block terminator");
                            verify_return();
                        }
                        break;
                    
                    default:
                        output_push(output, c, scope);
                }
                break;
            
            case '\\':
                /* When a backslash precedes a curly brace or another
                   backslash, print that character literally and skip this
                   backslash.  Otherwise, print this backslash literally. */
                switch (next) {                        
                    case '{':
                    case '}':
                    case '\\':
                        output_push(output, next, scope);
                        *offset += 2;
                        break;
                    default:
                        output_push(output, c, scope);
                        *offset += 1;
                }
                break;
            
            default:
                /* Write all other characters to the output buffer. */
                output_push(output, c, scope);
                *offset += 1;
        }
    }
    
    verify(scope == SCOPE_FILE, "unexpected EOF");
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
static int valid_root(json_t *root, json_error_t error)
{
    verify(root != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify(json_is_object(root), "root is not an object");
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup free(output)
static int jsontpl_jansson(json_t *root, char *template, char **out)
{
    autostr *output = autostr_new();
    size_t offset = 0;
    
    if (parse_scope(template, root, SCOPE_FILE, &offset, output)) {
#if VERIFY_LOG
        size_t line_count = 1,
               line_columns = 1,
               current_offset;
        for (current_offset = 0; current_offset < offset; current_offset++) {
            if (template[current_offset] == '\n') {
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
    
    *out = output->ptr;
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
    verify_bare(filesize_ftell != -1L);
    rewind(template_file);
    
    // Read template into memory
    template = malloc(filesize_ftell + 1);
    template[filesize_ftell] = '\0';
    filesize_fread = fread(template, 1, filesize_ftell, template_file);
    verify_bare(filesize_ftell == filesize_fread);
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