add_custom_target(sopranopm ALL ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Soprano.pm ${CMAKE_BINARY_DIR}/perl/blib/lib/Soprano.pm)
install(FILES Soprano.pm DESTINATION ${CUSTOM_PERL_SITE_ARCH_DIR}/)
