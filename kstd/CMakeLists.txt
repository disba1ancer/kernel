add_library(kstd STATIC
    include/cstdlib
    include/cstring
    include/kstd/btstdbeg.h
    include/kstd/btstdend.h
    include/kstd/stdlib_impl.h
    include/stdlib.h
    include/string.h
    qsort.c
    stdlib.c
)
target_include_directories(kstd PUBLIC include)
