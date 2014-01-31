#ifndef JSONTPL_H
#define JSONTPL_H

#define JSONTPL_ERROR_MESSAGE "[%s:%d] test failed: %s\n"
#define JSONTPL_CHECK_JSON_MESSAGE "ERROR: (%s) %s (line %d, column %d)\n"
#define JSONTPL_DUMPS_FLAGS (JSON_ENSURE_ASCII | JSON_ENCODE_ANY)

#ifdef JSONTPL_MAIN
#define jsontpl_log(...) fprintf(stderr, __VA_ARGS__)
static int jsontpl_log_va(int predicate, ...)
{
	va_list arg;
	va_start(arg, predicate);
    char *format = va_arg(arg, char *);
	int rtn = vfprintf(stderr, format, arg);
	va_end(arg);
	return rtn;
}
static int jsontpl_has_error_message(int predicate, ...)
{
    va_list arg;
    va_start(arg, predicate);
    char *format = va_arg(arg, char *);
    int has_format = (format != NULL);
    va_end(arg);
    return has_format;
}
#else
#define jsontpl_log(...)
#define jsontpl_log_va(...)
#define jsontpl_has_error_message(...) 0
#endif

#define jsontpl_predicate(predicate, ...) predicate
#define jsontpl_predicate_str(predicate, ...) #predicate
#define jsontpl_narg__(_1, _2, _3, _4, _5, _6, _7, n, ...) n
#define jsontpl_narg_(...) jsontpl_narg__(__VA_ARGS__)
#define jsontpl_narg(...) jsontpl_narg_(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0)

#define jsontpl_log_failure(...) do {                                   \
    if (jsontpl_has_error_message(__VA_ARGS__)) {                       \
        jsontpl_log("ERROR: ");                                         \
        jsontpl_log_va(__VA_ARGS__);                                    \
        jsontpl_log("\n");                                              \
    }                                                                   \
} while (0)

#define jsontpl_assert(...) do {                                        \
    int predicate = jsontpl_predicate(__VA_ARGS__, 0);                  \
    if (!(predicate)) {                                                 \
        if (jsontpl_narg(__VA_ARGS__) == 1) {                           \
            jsontpl_log(JSONTPL_ERROR_MESSAGE, __FILE__,                \
                __LINE__, jsontpl_predicate_str(__VA_ARGS__, 0));       \
        } else {                                                        \
            jsontpl_log_failure(__VA_ARGS__);                           \
        }                                                               \
        return __LINE__;                                                \
    }                                                                   \
} while (0)

#define jsontpl_fail(...) do {                                          \
    jsontpl_log_failure(0, __VA_ARGS__);                                \
    return __LINE__;                                                    \
} while (0)

#define jsontpl_check_json(predicate, error) do {                       \
    if (!(predicate)) {                                                 \
        jsontpl_log(JSONTPL_CHECK_JSON_MESSAGE, #predicate,             \
            error.text, error.line, error.column);                      \
        return __LINE__;                                                \
    }                                                                   \
} while (0)

#define jsontpl_isident(c) (isalnum(c) || isblank(c) || c == '_')

#define jsontpl_exit_scope(terminator) do {                             \
    jsontpl_assert(scope == terminator, "expected %s but reached %s",   \
        jsontpl_scope_terminator[scope],                                \
        jsontpl_scope_terminator[terminator]);                          \
    *output = buffer->ptr;                                              \
    free(buffer);                                                       \
    free(name);                                                         \
    free(key);                                                          \
    free(value);                                                        \
    return 0;                                                           \
} while (0)

#define jsontpl_assert_seq(predicate, ...) \
    jsontpl_assert(predicate, "invalid sequence: " __VA_ARGS__)

#define jsontpl_call(expr) do {                                         \
    int result = expr;                                                  \
    jsontpl_assert(!result, "call failed at line %d", result);          \
} while (0)

typedef enum {
    JSONTPL_INVALID_FILE,
    JSONTPL_OUT_OF_MEMORY,
    JSONTPL_READ_ERROR,
} jsontpl_return_code;

int jsontpl(char *json_filename, char *template_filename, char **output);

#endif