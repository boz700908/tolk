# Modern .NET build using dotnet CLI
# .NET SDK is installed via GitHub Actions setup-dotnet

find_program(DOTNET dotnet DOC ".NET CLI")
if(NOT DOTNET)
  message(WARNING ".NET CLI (dotnet) not found, skipping .NET wrapper build")
  return()
endif()

message(STATUS "Found .NET CLI: ${DOTNET}")

set(DOTNET_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/TolkDotNet.csproj")
set(DOTNET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/publish")
set(DOTNET_OUTPUT "${DOTNET_OUTPUT_DIR}/TolkDotNet.dll")

# Build .NET wrapper using dotnet build
add_custom_command(
  OUTPUT ${DOTNET_OUTPUT}
  COMMAND ${DOTNET} build ${DOTNET_PROJECT}
    --configuration Release
    --no-restore
    --output "${DOTNET_OUTPUT_DIR}"
    -p:TargetFramework=netstandard2.0
  DEPENDS ${DOTNET_PROJECT} "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs"
  COMMENT "Building .NET wrapper (TolkDotNet.dll) using dotnet build"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(TolkDotNet ALL DEPENDS ${DOTNET_OUTPUT})

# Copy .NET wrapper to dist directory
add_custom_command(TARGET TolkDotNet POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/dotnet"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DOTNET_OUTPUT}
    "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release>/dotnet/"
  COMMENT "Copying .NET wrapper to dist/${TOLK_ARCH}-$<CONFIG>/dotnet"
)

install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
