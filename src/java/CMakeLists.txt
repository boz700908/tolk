find_package(Java COMPONENTS Development)
if(NOT Java_FOUND)
  message(WARNING "Java Development Kit not found, skipping Java JAR build")
  return()
endif()

set(JAVA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/com/davykager/tolk/Tolk.java")
set(JAVA_CLASS_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
set(JAR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Tolk.jar")

# Java 8 compatibility mode (system requirements unchanged)
# --release 8: Proper cross-compilation (Java 9+ standard)
# - Automatically sets correct bootstrap classpath
# - Ensures only Java 8 APIs are used
# - No warnings, bytecode fully compatible with Java 8+
add_custom_command(
  OUTPUT ${JAVA_CLASS_DIR}/com/davykager/tolk/Tolk.class
  COMMAND ${CMAKE_COMMAND} -E make_directory ${JAVA_CLASS_DIR}
  COMMAND ${Java_JAVAC_EXECUTABLE}
    --release 8
    -Xlint:all
    -g:none -O
    -d ${JAVA_CLASS_DIR}
    ${JAVA_SOURCE}
  DEPENDS ${JAVA_SOURCE}
  COMMENT "Compiling Java source (Java 8+ compatible)"
  VERBATIM
)

add_custom_command(
  OUTPUT ${JAR_OUTPUT}
  COMMAND ${Java_JAR_EXECUTABLE}
    --create --file ${JAR_OUTPUT}
    -C ${JAVA_CLASS_DIR} .
  DEPENDS ${JAVA_CLASS_DIR}/com/davykager/tolk/Tolk.class
  COMMENT "Creating Tolk.jar (Java 8 bytecode)"
  VERBATIM
)

add_custom_target(TolkJar ALL DEPENDS ${JAR_OUTPUT})

# Copy Java wrapper to dist wrappers directory
add_custom_command(TARGET TolkJar POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/wrappers"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${JAR_OUTPUT}
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/wrappers/"
  COMMENT "Copying Java wrapper to dist/${TOLK_ARCH}-$<CONFIG>/wrappers"
)

install(FILES ${JAR_OUTPUT} DESTINATION bin OPTIONAL)
