# Prefer modern Roslyn compiler from .NET SDK (supports latest C# features)
# Fall back to .NET Framework csc for compatibility
set(PROGRAM_FILES_X86 "$ENV{ProgramFiles(x86)}")
set(PROGRAM_FILES "$ENV{ProgramFiles}")
set(WINDIR "$ENV{WINDIR}")

find_program(CSC csc
  PATHS
    # .NET SDK locations (modern Roslyn compiler, C# 12+)
    "${PROGRAM_FILES}/dotnet/sdk/*/Roslyn/bincore"
    "${PROGRAM_FILES_X86}/Microsoft Visual Studio/*/BuildTools/MSBuild/*/Bin/Roslyn"
    "${PROGRAM_FILES}/Microsoft Visual Studio/*/BuildTools/MSBuild/*/Bin/Roslyn"
    # .NET Framework locations (legacy C# 5.0 compiler)
    "${WINDIR}/Microsoft.NET/Framework64/v4.0.30319"
    "${WINDIR}/Microsoft.NET/Framework/v4.0.30319"
  DOC "C# compiler (Roslyn preferred)"
)

if(NOT CSC)
  message(WARNING "C# compiler (csc) not found, skipping .NET wrapper build")
  return()
endif()

message(STATUS "Found C# compiler: ${CSC}")

set(DOTNET_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs")
set(DOTNET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TolkDotNet.dll")

# Use latest C# language version, but target .NET Framework 4.0 for maximum compatibility
# /langversion:latest enables all C# features (expression-bodied members, etc.)
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
