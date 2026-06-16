find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(WARNING "Python 3 interpreter not found, skipping Python bytecode compilation")
  return()
endif()

set(PYTHON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.py")
set(PYC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Tolk.pyc")
set(PYO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Tolk.pyo")

# Python 3.5+ bytecache directory (in BINARY dir, not SOURCE dir)
set(PYCACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/__pycache__")
set(PYC_CACHED "${PYCACHE_DIR}/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.pyc")
set(PYO_CACHED "${PYCACHE_DIR}/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.opt-2.pyc")

# Compile standard Python bytecode (.pyc)
add_custom_command(
  OUTPUT ${PYC_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PYCACHE_DIR}
  COMMAND ${Python3_EXECUTABLE} -m py_compile ${PYTHON_SOURCE}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/__pycache__/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.pyc"
    ${PYC_OUTPUT}
  DEPENDS ${PYTHON_SOURCE}
  COMMENT "Compiling Python bytecode (.pyc)"
  VERBATIM
)

# Compile optimized Python bytecode (.pyo) using -OO (Python 3.5+)
# -O = remove assert, -OO = also remove docstrings
add_custom_command(
  OUTPUT ${PYO_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PYCACHE_DIR}
  COMMAND ${Python3_EXECUTABLE} -OO -m py_compile ${PYTHON_SOURCE}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/__pycache__/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.opt-2.pyc"
    ${PYO_OUTPUT}
  DEPENDS ${PYTHON_SOURCE}
  COMMENT "Compiling optimized Python bytecode (.pyo) with -OO"
  VERBATIM
)

add_custom_target(TolkPython ALL DEPENDS ${PYC_OUTPUT} ${PYO_OUTPUT})

# Copy Python wrapper and bytecodes to dist directory after build
add_custom_command(TARGET TolkPython POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/python"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PYTHON_SOURCE}
    ${PYC_OUTPUT}
    ${PYO_OUTPUT}
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/python/"
  COMMENT "Copying Python wrapper + bytecodes to dist/${TOLK_ARCH}-$<CONFIG>/python"
)

install(FILES ${PYTHON_SOURCE} ${PYC_OUTPUT} ${PYO_OUTPUT} DESTINATION bin/python OPTIONAL)
