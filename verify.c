#include <stdarg.h>
#include <stdio.h>

#include "verify.h"

#if VERIFY_LOG
int verify_log_va_(int _, ...)
{
    va_list arg;
    va_start(arg, _);
    char *format = va_arg(arg, char *);
    int rtn = vfprintf(VERIFY_LOG_FILE, format, arg);
    va_end(arg);
    return rtn;
}
#endif