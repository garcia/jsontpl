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
#else // VERIFY_LOG
#define verify_log_(...)
#endif // VERIFY_LOG

#define verify_tb_() \
    verify_log_("    in file %s, line %d\n", __FILE__, __LINE__)

#define verify_tb_hint_(...) do {                                           \
    verify_log_("    in file %s, line %d (", __FILE__, __LINE__);           \
    verify_log_(__VA_ARGS__);                                               \
    verify_log_(")\n");                                                     \
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
#define verify_fail(...) do {                                               \
    verify_log_("ERROR: ");                                                 \
    verify_log_(__VA_ARGS__);                                               \
    verify_log_("\n");                                                      \
    verify_tb_();                                                           \
    verify_cleanup;                                                         \
    return __LINE__;                                                        \
} while (0)

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

/* Same as verify_call_hint, but provide extra information about the call via
   printf arguments following the function call. */
#define verify_call_hint(expr, ...) do {                                    \
    if (expr) {                                                             \
        verify_tb_hint_(__VA_ARGS__);                                       \
        verify_cleanup;                                                     \
        return __LINE__;                                                    \
    }                                                                       \
} while (0)

/* Verify that the predicate is true.  If it is false, the remaining arguments
   are printf'd to VERIFY_LOG_FILE. */
#define verify(predicate, ...) do {                                         \
    if (!(predicate)) verify_fail(__VA_ARGS__);                             \
} while (0)

/* Verify that the predicate is true.  If it is false, the stringified
   predicate is logged to VERIFY_LOG_FILE. */
#define verify_bare(predicate) do {                                         \
    if (!(predicate)) verify_fail("check failed: %s", #predicate);          \
} while (0)

#endif // VERIFY_H