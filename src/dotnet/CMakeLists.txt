find_program(CSC csc DOC "C# compiler")

if(NOT CSC)
  message(WARNING "C# compiler (csc) not found, skipping .NET wrapper build")
  return()
endif()

set(DOTNET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TolkDotNet.dll")

add_custom_command(
  OUTPUT ${DOTNET_OUTPUT}
  COMMAND ${CSC} /nologo /optimize /target:library /warn:4
    /out:${DOTNET_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Tolk.cs
  COMMENT "Building .NET wrapper (TolkDotNet.dll)"
  VERBATIM
)

add_custom_target(TolkDotNet ALL DEPENDS ${DOTNET_OUTPUT})

install(FILES ${DOTNET_OUTPUT} DESTINATION bin OPTIONAL)
