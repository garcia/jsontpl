#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(JSONTPL_MAIN) && defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif // defined(JSONTPL_MAIN) && defined(_WIN32)

#include <jansson.h>

#include "autostr.h"
#include "cursor.h"
#include "jsontpl.h"
#include "jsontpl_filter.h"
#include "jsontpl_util.h"
#include "output.h"
#include "verify.h"

/**
 * Parse the template.  This function is responsible for writing literal text
 * and escape sequences to the output.  Values and blocks are passed to
 * parse_value and parse_block, respectively.
 */
static int parse_template(
        cursor_t *c,
        json_t *context,
        jsontpl_scope scope,
        output_t *out);

#undef verify_cleanup
#define verify_cleanup
/**
 * Discard spaces and tabs and set the cursor to the first non-blank character.
 */
static int discard_blank(cursor_t *c)
{
    char ch;
    
    while ((ch = cursor_peek(c)) != '\0') {
        if (isblank(ch)) {
            cursor_read(c);
        } else {
            verify_return();
        }
    }
    
    /* EOF is certainly an error here, but let the caller decide on the error
       message, because "EOF encountered while discarding whitespace" wouldn't
       be terribly helpful. */
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
/**
 * Discard characters until the sequence is found and set the cursor to the
 * first character following the sequence.
 */
static int discard_until(cursor_t *c, const char *seq)
{
    char ch;
    size_t seq_index = 0;
    
    while ((ch = cursor_peek(c)) != '\0') {
        
        if (seq[seq_index] == '\0') {
            verify_return();
        }
        
        if (ch == seq[seq_index]) {
            seq_index++;
        } else {
            seq_index = 0;
        }
        
        cursor_read(c);
    }
    
    verify_fail("expected '%s', got EOF", seq);
}

#undef verify_cleanup
#define verify_cleanup
/**
 * Read a sequence from the template, or raise an error if something other than
 * the given sequence is read.
 */
static int parse_seq(cursor_t *c, const char *seq)
{
    char ch;
    size_t seq_index = 0;
    
    verify_call(discard_blank(c));
    
    while ((ch = cursor_read(c)) != '\0') {
        
        verify(ch == seq[seq_index], "expected '%s', got '%c'", seq, ch);
        seq_index++;
        
        if (seq[seq_index] == '\0') {
            verify_return();
        }
    }
    
    verify_fail("expected '%s', got EOF", seq);
}

#undef verify_cleanup
#define verify_cleanup autostr_trim(identifier)
/**
 * Read an identifier (containing alphanumeric characters and underscores) from
 * the template and store it in `identifier`, or raise an error if the first
 * (non-blank) character is not an identifier character.
 */
static int parse_identifier(
        cursor_t *c,
        autostr_t *identifier)
{
    char ch;
    char seen_ident = 0;
    
    verify_call(discard_blank(c));
    
    while ((ch = cursor_peek(c)) != '\0') {
        
        if (isident(ch)) {
            seen_ident = 1;
            autostr_push(identifier, cursor_read(c));
        } else {
            verify(seen_ident, "expected an identifier, got '%c'", ch);
            verify_call(discard_blank(c));
            verify_return();
        }
    }
    
    verify_fail("EOF while reading identifier");
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&filter_name)
/**
 * Read a filter name from the template and process `obj` with that filter.
 */
int parse_filter(cursor_t *c, json_t **obj)
{
    autostr_t *filter_name = autostr();
    
    verify_call(parse_identifier(c, filter_name));
    verify_call(jsontpl_filter(filter_name, obj));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    autostr_free(&name);                                                    \
    json_decref(name_context);                                              \
    autostr_free(&variable_name);                                           \
    json_decref(variable_obj);                                              \
} while (0)
/**
 * Read a name from the template (which includes dot-separated identifiers and
 * optionally a filter), then store the object it identifies in `obj` and its
 * full name in `full_name`.
 */
static int parse_name(
        cursor_t *c,
        json_t *context,
        autostr_t *full_name,
        json_t **obj)
{
    char ch;
    autostr_t *name = autostr();
    json_t *name_context = context;
    autostr_t *variable_name = autostr();
    json_t *variable_obj = NULL;
    
    json_incref(name_context);
    
    verify_call(discard_blank(c));
    
    while ((ch = cursor_peek(c)) != '\0') {
        switch (ch) {
            
            case '{':
                /* Curly braces indicate variable names. Sorry for adding those. */
                cursor_read(c);
                verify_call(parse_name(c, context, variable_name, &variable_obj));
                verify_call(discard_blank(c));
                verify_call(parse_seq(c, "}"));
                verify(json_is_string(variable_obj), "%s: not a string", variable_name->ptr);
                autostr_append(name, json_string_value(variable_obj));
                json_decref(variable_obj);
                variable_obj = NULL;
                break;
            
            case '.':
                /* Periods indicate object subscripting.  The name up to the
                   period must be an object in the context.  That object then
                   becomes the context. */
                cursor_read(c);
                verify(autostr_len(name), "empty name component");
                verify_call(valid_name(name_context, name, full_name));
                *obj = json_object_get(name_context, name->ptr);
                verify(json_is_object(*obj), "%s: not an object", full_name->ptr);
                autostr_push(full_name, '.');
                json_incref(*obj);
                json_decref(name_context);
                name_context = *obj;
                autostr_recycle(&name);
                break;
            
            case '|':
                /* Pipes indicate filters. */
                cursor_read(c);
                verify_call(valid_name(name_context, name, full_name));
                *obj = json_object_get(name_context, name->ptr);
                json_incref(*obj);
                verify_call(parse_filter(c, obj));
                verify_return();
            
            default:
                if (isident(ch)) {
                    verify_call(parse_identifier(c, name));
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
/**
 * Read a value from the template and write it to the output.
 */
static int parse_value(
        cursor_t *c,
        json_t *context,
        jsontpl_scope scope,
        output_t *out)
{
    json_t *obj = NULL;
    autostr_t *full_name = autostr();
    
    if (scope & SCOPE_DISCARD) {
        verify_call(discard_until(c, "=}"));
    } else {
        verify_call(parse_name(c, context, full_name, &obj));
        verify_json_not_null(obj, full_name);
        verify_call(parse_seq(c, "=}"));
        verify_call(stringify_json(obj, full_name, out));
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(old_value);                                                 \
    autostr_free(&value);                                                   \
} while (0)
/**
 * Read the foreach block's value identifier, then parse the inner template with
 * each value in the array.
 */
static int parse_foreach_array(
        cursor_t *c,
        json_t *context,
        json_t *array,
        output_t *out)
{
    cursor_t starting_cursor;
    json_t *old_value = NULL;
    autostr_t *value = autostr();
    // Borrowed references; no need to free during cleanup]
    size_t array_index;
    json_t *array_value;
    
    verify_call(parse_identifier(c, value));
    verify_call(parse_seq(c, "%}"));
    
    if (json_array_size(array) == 0) {
        verify_call(parse_template(c, context, SCOPE_DISCARD, out));
        verify_return();
    }
    
    starting_cursor = *c;
    old_value = json_object_get(context, value->ptr);
    json_incref(old_value);
    
    json_array_foreach(array, array_index, array_value) {
        json_object_set(context, value->ptr, array_value);
        *c = starting_cursor;
        verify_call(parse_template(c, context, SCOPE_FOREACH, out));
    }
    
    json_object_set(context, value->ptr, old_value);
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(old_key);                                                   \
    json_decref(old_value);                                                 \
    autostr_free(&key);                                                     \
    autostr_free(&value);                                                   \
} while (0)
/**
 * Read the foreach block's key and value identifiers, then parse the inner
 * template with each key/value pair in the object.
 */
static int parse_foreach_object(
        cursor_t *c,
        json_t *context,
        json_t *object,
        output_t *out)
{
    cursor_t starting_cursor;
    json_t *old_key = NULL,
           *old_value = NULL;
    autostr_t *key = autostr(),
            *value = autostr();
    // Borrowed references; no need to free during cleanup
    const char *object_key;
    json_t *object_value;
    
    verify_call(parse_identifier(c, key));
    verify_call(parse_seq(c, "->"));
    verify_call(parse_identifier(c, value));
    verify_call(parse_seq(c, "%}"));
    
    if (json_object_size(object) == 0) {
        verify_call(parse_template(c, context, SCOPE_DISCARD, out));
        verify_return();
    }
    
    starting_cursor = *c;
    old_key = json_object_get(context, key->ptr);
    old_value = json_object_get(context, value->ptr);
    json_incref(old_key);
    json_incref(old_value);
    
    json_object_foreach(object, object_key, object_value) {
        json_object_set_new(context, key->ptr, json_string(object_key));
        json_object_set(context, value->ptr, object_value);
        *c = starting_cursor;
        verify_call(parse_template(c, context, SCOPE_FOREACH, out));
    }
    
    json_object_set(context, key->ptr, old_key);
    json_object_set(context, value->ptr, old_value);
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(obj);                                                       \
    autostr_free(&full_name);                                               \
} while (0)
/**
 * Pass control to parse_foreach_array or parse_foreach_object, depending on
 * the type of the given JSON value.
 */
static int parse_foreach(
        cursor_t *c,
        json_t *context,
        output_t *out)
{
    json_t *obj = NULL;
    autostr_t *full_name = autostr();
    
    verify_call(parse_name(c, context, full_name, &obj));
    verify_json_not_null(obj, full_name);
    verify_call(parse_seq(c, ":"));
    
    if (json_is_array(obj)) {
        verify_call(parse_foreach_array(c, context, obj, out));
    } else if (json_is_object(obj)) {
        verify_call(parse_foreach_object(c, context, obj, out));
    } else {
        verify_fail("foreach block requires an object or array");
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
/**
 * Discard characters until the matching end-block is reached.
 */
static int parse_comment(
        cursor_t *c,
        json_t *context,
        output_t *out)
{
    verify_call(parse_seq(c, "%}"));
    verify_call(parse_template(c, context, SCOPE_DISCARD | SCOPE_FORCE_DISCARD, out));
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(obj);                                                       \
    autostr_free(&full_name);                                               \
} while (0)
/**
 * Read a name and, if its value exists and is true, nonzero, or non-empty,
 * parse the inner template.
 */
static int parse_if(
        cursor_t *c,
        json_t *context,
        output_t *out)
{
    char discard = 0;
    jsontpl_scope scope = SCOPE_IF;
    json_t *obj = NULL;
    autostr_t *full_name = autostr();
    
    verify_call(parse_name(c, context, full_name, &obj));
    verify_call(parse_seq(c, "%}"));
    
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
    
    verify_call(parse_template(c, context, scope, out));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup autostr_free(&block_type)
/**
 * Read the block type.  If it's an end-block, `end` is set to 1 and control is
 * returned to parse_template.  If it's an else-block, the rest of the if-block
 * is parsed or discarded accordingly.  Other block types are passed along to
 * the corresponding function.
 */
static int parse_block(
        cursor_t *c,
        json_t *context,
        jsontpl_scope scope,
        output_t *out,
        char *end)
{
    *end = 0;
    jsontpl_scope inner_scope;
    size_t line = cursor_line(c),
           column = cursor_column(c);
    autostr_t *block_type = autostr();
    
    verify_call(parse_identifier(c, block_type));
    
    if (strcmp(block_type->ptr, "end") == 0) {
        verify_call(parse_seq(c, "%}"));
        *end = 1;
    
    } else if (strcmp(block_type->ptr, "else") == 0) {
        if (scope & SCOPE_FORCE_DISCARD) {
            /* Ignore nested if-else block when discarding input. */
            verify_call(discard_until(c, "%}"));
        } else if (scope & SCOPE_IF) {
            inner_scope = SCOPE_ELSE;
            if (!(scope & SCOPE_DISCARD)) {
                inner_scope |= SCOPE_DISCARD;
            }
            verify_call(parse_seq(c, "%}"));
            verify_call(parse_template(c, context, inner_scope, out));
            /* Unlike other blocks, else 'steals' the scope from its enclosing
               if block; when it ends, the if block ends as well. */
            *end = 1;
        } else {
            verify_fail("unexpected else marker");
        }
    
    } else if (scope & SCOPE_DISCARD) {
        verify_call(discard_until(c, "%}"));
        inner_scope = SCOPE_DISCARD;
        if (strcmp(block_type->ptr, "if") == 0) {
            /* Normally an else block is allowed to switch the scope from a
               discarding one to non-discarding and vice-versa, but if the
               scope in which the if/else block appears is already discarding,
               the entire block should be silenced. */
            inner_scope |= SCOPE_FORCE_DISCARD;
        }
        verify_call(parse_template(c, context, inner_scope, out));
            
    } else if (strcmp(block_type->ptr, "foreach") == 0) {
        verify_call_hint(parse_foreach(c, context, out),
            JSONTPL_BLOCK_HINT, "foreach", line, column);
        
    } else if (strcmp(block_type->ptr, "if") == 0) {
        verify_call_hint(parse_if(c, context, out),
            JSONTPL_BLOCK_HINT, "if", line, column);
        
    } else if (strcmp(block_type->ptr, "comment") == 0) {
        verify_call_hint(parse_comment(c, context, out),
            JSONTPL_BLOCK_HINT, "comment", line, column);
        
    } else {
        verify_fail("unknown block type '%s'", block_type->ptr);
    }
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup output_set_write(out, write_old)
static int parse_template(
        cursor_t *c,
        json_t *context,
        jsontpl_scope scope,
        output_t *out)
{
    char ch, end;
    char write_old = output_get_write(out);
    
    output_set_write(out, !(scope & SCOPE_DISCARD));
    
    while ((ch = cursor_read(c)) != '\0') {
        /* Useful for debugging:
           verify_log_("%c", ch); */
        
        switch (ch) {
            
            case '{':
                /* Curly braces indicate a value or a block. */
                switch (cursor_peek(c)) {
                    
                    case '=':
                        cursor_read(c);
                        verify_call(parse_value(c, context, scope, out));
                        break;
                    
                    case '%':
                        cursor_read(c);
                        verify_call(parse_block(c, context, scope, out, &end));
                        if (end) {
                            verify(scope != SCOPE_FILE, "unmatched block terminator");
                            verify_return();
                        }
                        break;
                    
                    default:
                        output_push(out, ch);
                }
                break;
            
            case '\\':
                /* When a backslash precedes a curly brace or another
                   backslash, print that character literally and skip this
                   backslash.  Otherwise, print this backslash literally. */
                switch (cursor_peek(c)) {                        
                    case '{':
                    case '}':
                    case '\\':
                        output_push(out, cursor_read(c));
                        break;
                    default:
                        output_push(out, ch);
                }
                break;
            
            default:
                /* Write all other characters to the output buffer. */
                output_push(out, ch);
        }
    }
    
    verify(scope == SCOPE_FILE, "unexpected EOF");
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup
/**
 * Common function used to verify whether a JSON object was successfully read.
 */
static int valid_root(json_t *root, json_error_t error)
{
    verify(root != NULL, JSONTPL_JSON_ERROR, error.text, error.line, error.column);
    verify(json_is_object(root), "root is not an object");
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup free(c)
static int jsontpl_jansson(json_t *root, char *template, output_t *out)
{
    cursor_t *c = cursor(template);
    
    verify_call_hint(parse_template(c, root, SCOPE_FILE, out),
        "reached line %d, column %d", cursor_line(c), cursor_column(c));
    
    verify_return();
}


/* Public functions: */


#undef verify_cleanup
#define verify_cleanup do {                                                 \
    json_decref(root);                                                      \
    output_free(&out);                                                      \
} while (0)
    
int jsontpl_string(char *json, char *template, char **output)
{
    json_error_t error;
    json_t *root = NULL;
    output_t *out = NULL;
    
    // Load JSON object from string
    root = json_loads(json, JSON_REJECT_DUPLICATES, &error);
    verify_call(valid_root(root, error)); 
    
    out = output_str(autostr());
    verify_call(jsontpl_jansson(root, template, out));
    *output = malloc(autostr_len(output_get_str(out)) + 1);
    strcpy(*output, autostr_value(output_get_str(out)));
    
    verify_return();
}

#undef verify_cleanup
#define verify_cleanup do {                                                 \
    if (template_file) fclose(template_file);                               \
    free(template);                                                         \
    json_decref(root);                                                      \
    output_free(&out);                                                      \
} while (0)
int jsontpl_file(char *json_filename, char *template_filename, FILE *outfile)
{
    json_error_t error;
    long filesize_ftell;
    size_t filesize_fread;
    char *template = NULL;
    json_t *root = NULL;
    FILE *template_file = NULL;
    output_t *out = NULL;
    
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
    
    out = output_file(outfile);
    
    verify_call(jsontpl_jansson(root, template, out));
    
    verify_return();
}

#ifdef JSONTPL_MAIN
/**
 * Main function for jsontpl.  Expects exactly three command-line arguments:
 * a JSON file path, a template file path, and an output file path.  Any parse
 * errors are reported on stderr.  Return code is 0 on success, 1 on invalid
 * argument count, or the last line number on which an error was reported.
 */
int main(int argc, char *argv[])
{
    // Check arg count
    if (argc != 3) {
        char *progname = "jsontpl";
        if (argc) progname = argv[0];
        fprintf(stderr, "USAGE: %s json-file template-file", progname);
        return 1;
    }
    
    // Set stdout to binary mode to avoid double-newlines
    #ifdef _WIN32
    _setmode(1,_O_BINARY);
    #endif // _WIN32
    
    return jsontpl_file(argv[1], argv[2], stdout);
}
#endif