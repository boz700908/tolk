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

# Build both target frameworks declared in TolkDotNet.csproj
# netstandard2.0 (modern .NET Core / .NET 5+) and net40 (.NET Framework 4.0+)
foreach(DOTNET_TFM netstandard2.0 net40)
  add_custom_command(
    OUTPUT "${DOTNET_OUTPUT_DIR}/netstandard2.0/TolkDotNet.dll"
    COMMAND ${DOTNET} build ${DOTNET_PROJECT}
      --configuration Release
      --no-restore
      --output "${DOTNET_OUTPUT_DIR}"
      -p:TargetFramework=${DOTNET_TFM}
    DEPENDS ${DOTNET_PROJECT} "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs"
    COMMENT "Building .NET wrapper (TolkDotNet.dll) for ${DOTNET_TFM} using dotnet build"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endforeach()

add_custom_target(TolkDotNet ALL DEPENDS "${DOTNET_OUTPUT_DIR}/netstandard2.0/TolkDotNet.dll")

# Note: .NET wrapper is copied once to wrappers/dotnet/ via GitHub Actions
# No per-architecture copy needed - wrappers are shared across all architectures
install(FILES "${DOTNET_OUTPUT_DIR}/netstandard2.0/TolkDotNet.dll" DESTINATION bin OPTIONAL)
