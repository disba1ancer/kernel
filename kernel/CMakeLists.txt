add_executable(kernel)

target_sources(kernel PRIVATE
    main.cpp
)

target_link_libraries(kernel PRIVATE kstd generic platform generic supc++ gcc platform)
target_link_options(kernel PRIVATE -T ${KERNEL_LINKER_SCRIPT})
set_target_properties(kernel PROPERTIES LINK_DEPENDS ${KERNEL_LINKER_SCRIPT})

install(TARGETS kernel RUNTIME)
