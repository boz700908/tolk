# Search for C# compiler in .NET Framework directories
find_program(CSC csc
  PATHS
    "$ENV{WINDIR}/Microsoft.NET/Framework64/v4.0.30319"
    "$ENV{WINDIR}/Microsoft.NET/Framework/v4.0.30319"
    "$ENV{WINDIR}/Microsoft.NET/Framework64/v3.5"
    "$ENV{WINDIR}/Microsoft.NET/Framework/v3.5"
  DOC "C# compiler"
)

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

install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
