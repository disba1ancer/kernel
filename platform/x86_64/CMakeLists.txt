add_library(platform_x86_64 STATIC EXCLUDE_FROM_ALL)
add_library(platform ALIAS platform_x86_64)

target_sources(platform_x86_64 PRIVATE
    alloc.cpp
    alloc.h
    alloc.s
    exit.c
    init.c
    init.cpp
    init.s
    interrupts.c
    interrupts.h
    interrupts.s
    memcmp.s
    memcpymove.s
    memset.s
    processor.c
    processor.h
    strchr.s
    strcmp.s
    strlen.s
)

target_link_libraries(platform_x86_64 PRIVATE kstd generic)
target_link_options(platform_x86_64 INTERFACE -z max-page-size=0x1000)

add_custom_target(crti
    ALL ${CMAKE_ASM_COMPILER} -Wa,--divide -g -c ${CMAKE_CURRENT_SOURCE_DIR}/crti.s -o ${CMAKE_BINARY_DIR}/crti.o
    SOURCES
        crti.s
    BYPRODUCTS
        ${CMAKE_BINARY_DIR}/crti.o
)
add_custom_target(crtn
    ALL ${CMAKE_ASM_COMPILER} -Wa,--divide -g -c ${CMAKE_CURRENT_SOURCE_DIR}/crtn.s -o ${CMAKE_BINARY_DIR}/crtn.o
    SOURCES
        crtn.s
    BYPRODUCTS
        ${CMAKE_BINARY_DIR}/crtn.o
)
