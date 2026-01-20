#ifndef UTILS_H
#define UTILS_H

#include <ctime>
#include <iostream>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

#if defined(DEBUG)
#define DB_PRINT(msg, ...) do {      \
    const auto tt {time(0)};         \
    std::cout << ctime(&tt) << "\t"; \
    printf(msg, __VA_ARGS__);        \
} while (0)

#define DB_PRINT_ARRAY(data, size) do {                 \
    for (__typeof(size) __i = 0; __i < (size); __i++) { \
        printf("%.2x%c", (data)[__i],                   \
                ((__i + 1) % 16 == 0) ? '\n' : ' ');    \
    }                                                   \
    printf("\n");                                       \
} while (0)
#else
#define DB_PRINT(msg, ...) do {} while (0)
#define DB_PRINT_ARRAY(data, size) do {} while(0)
#endif

#endif
