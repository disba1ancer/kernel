cmake_minimum_required(VERSION 3.25)

set(CMAKE_SYSTEM_NAME elf)
#set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

execute_process(
    COMMAND
        ${CMAKE_CXX_COMPILER} -mcmodel=large -mno-red-zone -print-file-name=crtbegin.o
    OUTPUT_VARIABLE KERNEL_CRTBEGIN
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND
        ${CMAKE_CXX_COMPILER} -mcmodel=large -mno-red-zone -print-file-name=crtend.o
    OUTPUT_VARIABLE KERNEL_CRTEND
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -nostdlib -ffreestanding -mcmodel=large -mno-red-zone -Wall -Wextra -pedantic -pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdlib -ffreestanding -mcmodel=large -mno-red-zone -Wall -Wextra -pedantic -pedantic-errors") # -fno-exceptions -fno-rtti

set(CMAKE_ASM_LINK_EXECUTABLE "<CMAKE_ASM_COMPILER> <FLAGS> <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${KERNEL_CRTBEGIN} <OBJECTS> -o <TARGET> <LINK_LIBRARIES> ${KERNEL_CRTEND} ${CMAKE_CURRENT_BINARY_DIR}/crtn.o")
set(CMAKE_C_LINK_EXECUTABLE   "<CMAKE_C_COMPILER>   <FLAGS> <CMAKE_C_LINK_FLAGS>   <LINK_FLAGS> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${KERNEL_CRTBEGIN} <OBJECTS> -o <TARGET> <LINK_LIBRARIES> ${KERNEL_CRTEND} ${CMAKE_CURRENT_BINARY_DIR}/crtn.o")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${KERNEL_CRTBEGIN} <OBJECTS> -o <TARGET> <LINK_LIBRARIES> ${KERNEL_CRTEND} ${CMAKE_CURRENT_BINARY_DIR}/crtn.o")

project(kernel LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(ldscripts)
add_subdirectory(kstd)
add_subdirectory(generic)
add_subdirectory(platform)
add_subdirectory(kernel)
