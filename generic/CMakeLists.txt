add_library(generic STATIC
    include/kernel/UniquePtr.hpp
    include/kernel/allocator.hpp
    include/kernel/avl_tree.hpp
    include/kernel/avl_tree_node.hpp
    include/kernel/bootdata.h
    include/kernel/list.hpp
    include/kernel/list_node.hpp
    include/kernel/node.hpp
    include/kernel/util.h
    include/kernel/util.hpp
    util.c
    util.cpp
)

target_link_libraries(generic PRIVATE kstd)
target_include_directories(generic PUBLIC include)
