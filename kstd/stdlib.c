#include "include/stdlib.h"

#define GENERATE(type, namepref)\
type namepref##abs(type val)\
{\
    return val < 0 ? -val : val;\
}

GENERATE(int, )
GENERATE(long, l)
GENERATE(long long, ll)
#undef GENERATE

#undef GENERATE
