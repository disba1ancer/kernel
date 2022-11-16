add_library(generic STATIC
    include/kernel/UniquePtr.hpp
    include/kernel/bootdata.h
    include/kernel/util.h
    include/kernel/util.hpp
    util.c
)

target_include_directories(generic PUBLIC include)
target_link_libraries(generic PUBLIC kstd)
