# Search for C# compiler - prefer modern Roslyn from .NET SDK
# .NET SDK is installed via GitHub Actions setup-dotnet, so Roslyn csc.exe is available
# Fall back to .NET Framework csc for local builds without .NET SDK
find_program(CSC csc
  PATHS
    # .NET SDK Roslyn compiler (modern C# 12+, installed by GitHub Actions)
    $ENV{DOTNET_ROOT}/sdk/*/Roslyn/bincore
    # Visual Studio Build Tools Roslyn
    [=[C:/Program Files/Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/Roslyn]=]
    [=[C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/Roslyn]=]
    # .NET Framework (legacy C# 5.0, fallback only)
    $ENV{WINDIR}/Microsoft.NET/Framework64/v4.0.30319
    $ENV{WINDIR}/Microsoft.NET/Framework/v4.0.30319
  DOC "C# compiler (Roslyn preferred)"
)

if(NOT CSC)
  message(WARNING "C# compiler (csc) not found, skipping .NET wrapper build")
  return()
endif()

message(STATUS "Found C# compiler: ${CSC}")

set(DOTNET_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs")
set(DOTNET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TolkDotNet.dll")

# Use latest C# language features when Roslyn is available
# Target framework remains .NET Framework 4.0 compatible
add_custom_command(
  OUTPUT ${DOTNET_OUTPUT}
  COMMAND ${CSC} /nologo /optimize /target:library /warn:4
    /langversion:latest
    "/out:${DOTNET_OUTPUT}" "${DOTNET_SOURCE}"
  DEPENDS ${DOTNET_SOURCE}
  COMMENT "Building .NET wrapper (TolkDotNet.dll) - C# latest"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(TolkDotNet ALL DEPENDS ${DOTNET_OUTPUT})
install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
