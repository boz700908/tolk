# Simple C# compiler search
# .NET SDK is installed via GitHub Actions setup-dotnet, csc.exe is in PATH
find_program(CSC csc DOC "C# compiler")

if(NOT CSC)
  message(WARNING "C# compiler (csc) not found, skipping .NET wrapper build")
  return()
endif()

message(STATUS "Found C# compiler: ${CSC}")

set(DOTNET_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs")
set(DOTNET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TolkDotNet.dll")

add_custom_command(
  OUTPUT ${DOTNET_OUTPUT}
  COMMAND ${CSC} /nologo /optimize /target:library /warn:4
    "/out:${DOTNET_OUTPUT}" "${DOTNET_SOURCE}"
  DEPENDS ${DOTNET_SOURCE}
  COMMENT "Building .NET wrapper (TolkDotNet.dll)"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(TolkDotNet ALL DEPENDS ${DOTNET_OUTPUT})

# Copy .NET wrapper to dist wrappers directory
add_custom_command(TARGET TolkDotNet POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/wrappers"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DOTNET_OUTPUT}
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/wrappers/"
  COMMENT "Copying .NET wrapper to dist/${TOLK_ARCH}-$<CONFIG>/wrappers"
)

install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
