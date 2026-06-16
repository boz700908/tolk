find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(WARNING "Python 3 interpreter not found, skipping Python bytecode compilation")
  return()
endif()

set(PYTHON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.py")
set(PYC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Tolk.pyc")
set(PYO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Tolk.pyo")

# Compile Python bytecode (.pyc)
add_custom_command(
  OUTPUT ${PYC_OUTPUT}
  COMMAND ${Python3_EXECUTABLE} -m py_compile ${PYTHON_SOURCE}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/__pycache__/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.pyc"
    ${PYC_OUTPUT}
  DEPENDS ${PYTHON_SOURCE}
  COMMENT "Compiling Python bytecode (.pyc)"
  VERBATIM
)

# Compile optimized Python bytecode (.pyo)
add_custom_command(
  OUTPUT ${PYO_OUTPUT}
  COMMAND ${Python3_EXECUTABLE} -O -m py_compile ${PYTHON_SOURCE}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/__pycache__/Tolk.cpython-${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.opt-1.pyc"
    ${PYO_OUTPUT}
  DEPENDS ${PYTHON_SOURCE}
  COMMENT "Compiling optimized Python bytecode (.pyo)"
  VERBATIM
)

add_custom_target(TolkPython ALL DEPENDS ${PYC_OUTPUT} ${PYO_OUTPUT})

install(FILES ${PYTHON_SOURCE} ${PYC_OUTPUT} ${PYO_OUTPUT} DESTINATION bin/python OPTIONAL)
