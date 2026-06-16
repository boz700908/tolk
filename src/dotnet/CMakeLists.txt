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

# Note: .NET wrapper is copied once to wrappers/dotnet/ via GitHub Actions
# No per-architecture copy needed - wrappers are shared across all architectures
install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
