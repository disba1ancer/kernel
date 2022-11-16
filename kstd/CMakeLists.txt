add_library(kstd STATIC
    qsort.c
    include/btstdbeg.h
    include/btstdend.h
    include/string.h
    include/stdlib.h
)
target_include_directories(kstd PUBLIC include)
