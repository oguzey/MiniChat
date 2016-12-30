#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define GREEN   "\033[01;32m"
#define YELLOW  "\033[01;33m"
#define RED     "\033[01;31m"
#define RESET_COLOR "\033[0m"
#define PAINT(word, color) color word RESET_COLOR


__attribute__((format(printf, 2, 3)))
inline static void _log(const char *level, const char *format, ...)
{
    char buffer[2048] = {0};
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    fprintf(stdout, "%s : %s\n", level, buffer);
    va_end(args);
    fflush(stdout);
}

#define info(...) _log(PAINT("info", GREEN), __VA_ARGS__)
#define warn(...) _log(PAINT("warn", RED), __VA_ARGS__)
#define fatal(...) _log(PAINT("fatal", RED), __VA_ARGS__); exit(EXIT_FAILURE)

#ifdef NDEBUG
#define debug(format, ...)
#else
#define debug(...) _log(PAINT("debug", YELLOW), __VA_ARGS__)
#endif /* NDEBUG */
