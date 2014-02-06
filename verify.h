#ifndef VERIFY_H
#define VERIFY_H

#ifndef VERIFY_LOG
#define VERIFY_LOG 1
#endif // VERIFY_LOG

#ifndef VERIFY_LOG_FILE
#define VERIFY_LOG_FILE stderr
#endif // VERIFY_LOG_FILE

#define VERIFY_GENERIC_ERROR "check failed: %s\n"

#if VERIFY_LOG
#define verify_log_(...) fprintf(VERIFY_LOG_FILE, __VA_ARGS__)
int verify_log_va_(int _, ...);
#else // VERIFY_LOG
#define verify_log_(...)
#define verify_log_va_(...)
#endif // VERIFY_LOG

#define verify_predicate_(predicate, ...) predicate
#define verify_predicate_str_(predicate, ...) #predicate
#define verify_arg2_(_, ...) __VA_ARGS__
#define verify_narg___(_1, _2, _3, _4, _5, _6, _7, n, ...) n
#define verify_narg__(...) verify_narg___(__VA_ARGS__)
#define verify_narg_(...) verify_narg__(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0)

#define verify_tb_() \
    verify_log_("    in file %s, line %d\n", __FILE__, __LINE__)

#define verify_fail_with_predicate_(...) do {                               \
    verify_log_("ERROR: ");                                                 \
    verify_log_va_(__VA_ARGS__);                                            \
    verify_log_("\n");                                                      \
    verify_tb_();                                                           \
    verify_cleanup;                                                         \
    return __LINE__;                                                        \
} while (0)


/* Public macros: */


/* Redefine this macro to a cleanup sequence around function blocks.  The
   sequence will be called immediately prior to any verify-invoked return. */
#define verify_cleanup

/* Return a non-error code.  Use this instead of returning 0 manually to invoke
   the cleanup sequence. */
#define verify_return() do {                                                \
    verify_cleanup;                                                         \
    return 0;                                                               \
} while (0)

/* Print the error message and return an error code.  Use this in places where
   the verify macro cannot be used, e.g. switch cases. */
#define verify_fail(...) verify_fail_with_predicate_(0, __VA_ARGS__)

/* Evaluate the expression (which should be a function call) and, if it returns
   an error code, print a traceback to the log file and return an error code.
   Wrap all calls to functions that use the verify pattern in this macro. */
#define verify_call(expr) do {                                              \
    if (expr) {                                                             \
        verify_tb_();                                                       \
        verify_cleanup;                                                     \
        return __LINE__;                                                    \
    }                                                                       \
} while (0)

/* Verify that the first argument (the predicate) is true.  The predicate must
   be an integer, so pointers must be explicitly compared to NULL.  If it is
   false, an error message is printed to VERIFY_LOG_FILE and an error code is
   returned.  If there are arguments following the predicate, they are printf'd
   to the error message; otherwise, the error is the stringified predicate. */
#define verify(...) do {                                                    \
    int predicate = (verify_predicate_(__VA_ARGS__, 0));                    \
    if (!(predicate)) {                                                     \
        if (verify_narg_(__VA_ARGS__) == 1) {                               \
            verify_fail("check failed: %s",                                 \
                verify_predicate_str_(__VA_ARGS__, 0));                     \
        } else verify_fail_with_predicate_(__VA_ARGS__);                    \
    }                                                                       \
} while (0)

#endif // VERIFY_H