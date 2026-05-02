find_program(PANDOC pandoc DOC "Pandoc document converter")

if(NOT PANDOC)
  message(WARNING "Pandoc not found, skipping documentation build")
  return()
endif()

set(DOC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/README.html")

add_custom_command(
  OUTPUT ${DOC_OUTPUT}
  COMMAND ${PANDOC} -s --toc
    -M title="Tolk" -M author="Davy Kager"
    -r markdown -w html5
    -o ${DOC_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  COMMENT "Generating HTML documentation"
  VERBATIM
)

add_custom_target(TolkDocs ALL DEPENDS ${DOC_OUTPUT})

install(FILES ${DOC_OUTPUT} DESTINATION doc OPTIONAL)
