#include <stdio.h>

#define GREEN "\e[01;32m"
#define YELLOW "\e[01;33m"
#define RESET_COLOR "\e[0m"
#define PAINT(word, color) color word RESET_COLOR

#ifdef NDEBUG

#define info(format, ...) printf(PAINT("info", GREEN) " : " format "\n", ##__VA_ARGS__)
#define debug(format, ...)

#else

#define info(format, ...) printf(PAINT("info", GREEN) " [%s] : " format "\n", __func__,  ##__VA_ARGS__)
#define debug(format, ...) printf(PAINT("debug", YELLOW) " [%s:%d] : " format "\n", __func__, __LINE__,  ##__VA_ARGS__)

#endif /* NDEBUG */