add_executable(kernel)

target_sources(kernel PRIVATE
    main.cpp
)

add_dependencies(kernel crti crtn)
target_link_libraries(kernel PRIVATE platform supc++ gcc platform)
target_link_options(kernel PRIVATE -T ${KERNEL_LINKER_SCRIPT})
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${KERNEL_LINKER_SCRIPT})
