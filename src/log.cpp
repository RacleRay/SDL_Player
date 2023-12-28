#include "log.h"

#include <cstdarg>
#include <cstdio>

extern "C" {

    void ff_log(char *format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }

    void ff_log_line(const char *format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        printf("\n");
        va_end(args);

        (void)fflush(stdout);
    }
}