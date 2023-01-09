add_library(generic STATIC
    include/kernel/UniquePtr.hpp
    include/kernel/avl_tree.h
    include/kernel/bootdata.h
    include/kernel/list.h
    include/kernel/node.h
    include/kernel/util.h
    include/kernel/util.hpp
    util.c
)

target_link_libraries(generic PRIVATE kstd)
target_include_directories(generic PUBLIC include)
