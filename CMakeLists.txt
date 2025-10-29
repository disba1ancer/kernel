cmake_minimum_required(VERSION 3.5)

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -nodefaultlibs")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -nodefaultlibs -ffreestanding -fpie -mno-red-zone -Wall -Wextra -pedantic -pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nodefaultlibs -ffreestanding -fpie -mno-red-zone -Wall -Wextra -pedantic -pedantic-errors")

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
