# Wrapper binding compile checks.
#
# The wrappers are source-only in a normal build, so a renamed or removed
# Tolk_* export could break them without anyone noticing. This project is built
# only with -DTOLK_BUILD_WRAPPER_TESTS=ON: it compiles the bindings that do not
# need a game engine SDK, links the GameMaker shim against the real Tolk target
# and runs a smoke test. Nothing here is installed or shipped.

# Every Tolk_* name a binding calls must exist in src/Tolk.h.
set(TOLK_WRAPPER_BINDING_FILES
  "${CMAKE_SOURCE_DIR}/contrib/game-engines/unity/Runtime/Tolk.cs"
  "${CMAKE_SOURCE_DIR}/contrib/game-engines/gamemaker/source/tolk_gml.c"
  "${CMAKE_SOURCE_DIR}/contrib/lua/Tolk.lua"
  "${CMAKE_SOURCE_DIR}/src/python/Tolk.py"
)
include("${CMAKE_CURRENT_SOURCE_DIR}/check_bindings.cmake")

# Smoke test: load the shim, call it, unload it. The shim itself is built by
# the normal build (see wrappers/CMakeLists.txt) and linked against the real
# Tolk target there, so a missing export fails that link.
add_executable(tolk_wrappers_test tolk_wrappers_test.c)
target_link_libraries(tolk_wrappers_test PRIVATE TolkGml)
add_custom_command(TARGET tolk_wrappers_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Tolk>" "$<TARGET_FILE_DIR:tolk_wrappers_test>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:TolkGml>" "$<TARGET_FILE_DIR:tolk_wrappers_test>"
  COMMAND "$<TARGET_FILE:tolk_wrappers_test>"
  COMMENT "Running the wrapper smoke test"
  VERBATIM
)

# Unity C# binding: compiled with the .NET SDK. The project is generated into
# the build tree so no obj/ or bin/ ends up next to the sources.
find_program(DOTNET_EXE dotnet DOC ".NET CLI")
if(DOTNET_EXE)
  file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/contrib/game-engines/unity/Runtime/Tolk.cs" TOLK_UNITY_BINDING)
  # Keep the generated project, obj/ and bin/ out of the folder that holds the
  # generated vcxproj files: a stray project.assets.json next to a C++ project
  # makes MSBuild's NuGet targets fail.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/csharp")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/csharp/TolkWrapperCompile.csproj.in"
    "${CMAKE_CURRENT_BINARY_DIR}/csharp/TolkWrapperCompile.csproj"
    @ONLY
  )
  add_custom_target(TolkUnityBindingCompile ALL
    COMMAND "${DOTNET_EXE}" build "${CMAKE_CURRENT_BINARY_DIR}/csharp/TolkWrapperCompile.csproj" --configuration Release --nologo
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/csharp"
    COMMENT "Compiling the Unity C# binding (compile check)"
    VERBATIM
  )
else()
  message(WARNING "dotnet not found: skipping the Unity C# binding compile check")
endif()
