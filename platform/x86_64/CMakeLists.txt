add_library(platform_x86_64 OBJECT EXCLUDE_FROM_ALL)
add_library(platform ALIAS platform_x86_64)

target_sources(platform_x86_64 PRIVATE
    alloc.cpp
    alloc.h
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
    segment.c
    segment.h
    strchr.s
    strcmp.s
    strlen.s
)

target_link_libraries(platform_x86_64 PUBLIC kstd generic)
target_link_options(platform_x86_64 INTERFACE -z max-page-size=0x1000 -B ${CMAKE_BINARY_DIR} -specs=${CMAKE_CURRENT_SOURCE_DIR}/specs.txt)

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
# add_library(crti OBJECT crti.s)
# target_compile_options(crti PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wa,--divide -g>)
# add_library(crtn OBJECT crtn.s)
# target_compile_options(crtn PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wa,--divide -g>)

add_dependencies(platform_x86_64 crti crtn)
