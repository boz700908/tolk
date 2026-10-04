# Fails the configure step when a binding calls a Tolk_* entry point that is no
# longer declared in src/Tolk.h, so renaming or removing an export cannot
# silently break a wrapper. Runs only for TOLK_BUILD_WRAPPER_TESTS builds.

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/Tolk.h")
  message(FATAL_ERROR "Tolk.h not found: cannot check the wrapper bindings")
endif()
file(READ "${CMAKE_SOURCE_DIR}/src/Tolk.h" TOLK_HEADER_TEXT)

foreach(TOLK_BINDING IN LISTS TOLK_WRAPPER_BINDING_FILES)
  if(NOT EXISTS "${TOLK_BINDING}")
    message(FATAL_ERROR "wrapper binding file not found: ${TOLK_BINDING}")
  endif()
  file(READ "${TOLK_BINDING}" TOLK_BINDING_TEXT)
  string(REGEX MATCHALL "Tolk_[A-Za-z0-9_]+" TOLK_BINDING_SYMBOLS "${TOLK_BINDING_TEXT}")
  if(TOLK_BINDING_SYMBOLS)
    list(REMOVE_DUPLICATES TOLK_BINDING_SYMBOLS)
  endif()
  foreach(TOLK_SYMBOL IN LISTS TOLK_BINDING_SYMBOLS)
    string(FIND "${TOLK_HEADER_TEXT}" "${TOLK_SYMBOL}(" TOLK_SYMBOL_POS)
    if(TOLK_SYMBOL_POS EQUAL -1)
      message(FATAL_ERROR "${TOLK_BINDING} calls ${TOLK_SYMBOL}, which is not declared in src/Tolk.h")
    endif()
  endforeach()
  message(STATUS "Wrapper symbols OK: ${TOLK_BINDING}")
endforeach()