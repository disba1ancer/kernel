#include <stdlib.h>

#define GENERATE(type, namepref)\
extern inline type namepref##abs(type val);

GENERATE(int, )
GENERATE(long, l)
GENERATE(long long, ll)
GENERATE(intmax_t, imax)

#undef GENERATE
