set(TOLK_SOURCES
  Tolk.cpp
  TolkBridge.cpp
  ScreenReaderDriverBOY.cpp
  ScreenReaderDriverJAWS.cpp
  ScreenReaderDriverNVDA.cpp
  ScreenReaderDriverSA.cpp
  ScreenReaderDriverSAPI.cpp
  ScreenReaderDriverSNova.cpp
  ScreenReaderDriverWE.cpp
  ScreenReaderDriverZDCloud.cpp
  ScreenReaderDriverZDSR.cpp
  ScreenReaderDriverZT.cpp
  fsapi.c
  wineyes.c
  zt.c
)
set(TOLK_HEADERS
  Tolk.h
  TolkVersion.h
  TolkDebug.h
  TolkBridge.h
  ScreenReaderDriver.h
  ScreenReaderDriverBridged.h
  ScreenReaderDriverBOY.h
  ScreenReaderDriverJAWS.h
  ScreenReaderDriverNVDA.h
  ScreenReaderDriverSA.h
  ScreenReaderDriverSAPI.h
  ScreenReaderDriverSNova.h
  ScreenReaderDriverWE.h
  ScreenReaderDriverZDCloud.h
  ScreenReaderDriverZDSR.h
  ScreenReaderDriverZT.h
  fsapi.h
  wineyes.h
  zt.h
)
# JNI support
if(TOLK_BUILD_JNI)
  find_package(JNI QUIET)
  if(JNI_FOUND)
    list(APPEND TOLK_SOURCES TolkJNI.cpp)
    message(STATUS "JNI found, building with Java Native Interface support")
  else()
    message(WARNING "JNI not found, skipping Java Native Interface support")
    set(TOLK_BUILD_JNI OFF)
  endif()
endif()
# DLL target
add_library(Tolk SHARED ${TOLK_SOURCES} ${TOLK_HEADERS} Tolk.rc)
target_compile_definitions(Tolk PRIVATE
  _EXPORTING
  UNICODE
  _UNICODE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<BOOL:${TOLK_BUILD_JNI}>:_WITH_JNI>
)
if(TOLK_BUILD_JNI)
  target_include_directories(Tolk PRIVATE ${JNI_INCLUDE_DIRS})
endif()
target_link_libraries(Tolk PRIVATE
  User32
  Ole32
  OleAut32
)
# MSVC-specific settings - per configuration
if(MSVC)
  target_compile_options(Tolk PRIVATE
    /W4
    /EHsc
    /Gw
    $<$<CONFIG:Debug>:/Od /Zi /RTC1 /RTCsu>
    $<$<CONFIG:Release>:/O2 /GL>
  )
  target_link_options(Tolk PRIVATE
    $<$<CONFIG:Debug>:/DEBUG:FULL>
    $<$<CONFIG:Release>:/LTCG>
  )
endif()
# --- Cross-architecture bridge for backends without a module for this arch ---
# Some backends only ship a DLL for another architecture: SuperNova and ZDCloud
# are 32-bit only, while System Access, ZDSR and BoyPCReader ship 32-bit and
# 64-bit builds but no ARM64 build. A build that cannot load such a module
# in-process drives it through a small helper process of the matching
# architecture. The helper image and the modules it loads are embedded into
# Tolk.dll as resources and extracted to a per-user cache directory at run
# time, so no extra file is shipped next to Tolk.dll. Keep the resource ids and
# file names in sync with the payload tables in src/TolkBridge.cpp.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  include(ExternalProject)
  set(TOLK_BRIDGE_RC_CONTENT "")
  set(TOLK_BRIDGE_DEPENDS "")
  if(CMAKE_GENERATOR_TOOLSET)
    set(TOLK_BRIDGE_TOOLSET_ARGS CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}")
  else()
    set(TOLK_BRIDGE_TOOLSET_ARGS "")
  endif()
  # rc.exe treats backslashes as escape characters, so double them.
  macro(tolk_bridge_embed_file id path)
    file(TO_NATIVE_PATH "${path}" TOLK_BRIDGE_NATIVE_PATH)
    string(REPLACE "\\" "\\\\" TOLK_BRIDGE_NATIVE_PATH "${TOLK_BRIDGE_NATIVE_PATH}")
    string(APPEND TOLK_BRIDGE_RC_CONTENT "${id} RCDATA \"${TOLK_BRIDGE_NATIVE_PATH}\"\n")
  endmacro()
  # 32-bit helper: used by x64, ARM64EC and ARM64 builds.
  if(TOLK_ARCH STREQUAL "x64" OR TOLK_ARCH STREQUAL "arm64ec" OR TOLK_ARCH STREQUAL "arm64")
    set(TOLK_BRIDGE_X86_DIR "${CMAKE_BINARY_DIR}/bridge-x86")
    set(TOLK_BRIDGE_X86_EXE "${TOLK_BRIDGE_X86_DIR}/TolkBridge.exe")
    ExternalProject_Add(tolk_bridge_x86
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
      BINARY_DIR "${TOLK_BRIDGE_X86_DIR}"
      CMAKE_GENERATOR "${CMAKE_GENERATOR}"
      CMAKE_GENERATOR_PLATFORM "Win32"
      ${TOLK_BRIDGE_TOOLSET_ARGS}
      BUILD_COMMAND "${CMAKE_COMMAND}" --build "${TOLK_BRIDGE_X86_DIR}" --config Release
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS "${TOLK_BRIDGE_X86_EXE}"
    )
    tolk_bridge_embed_file(101 "${TOLK_BRIDGE_X86_EXE}")
    list(APPEND TOLK_BRIDGE_DEPENDS tolk_bridge_x86)
    tolk_bridge_embed_file(250 "${CMAKE_SOURCE_DIR}/libs/x86/dolapi32.dll")
    tolk_bridge_embed_file(280 "${CMAKE_SOURCE_DIR}/libs/x86/ZDCloudAPI.dll")
  endif()
  # 64-bit helper: used by ARM64 builds, which cannot load x64 modules.
  if(TOLK_ARCH STREQUAL "arm64")
    set(TOLK_BRIDGE_X64_DIR "${CMAKE_BINARY_DIR}/bridge-x64")
    set(TOLK_BRIDGE_X64_EXE "${TOLK_BRIDGE_X64_DIR}/TolkBridge.exe")
    ExternalProject_Add(tolk_bridge_x64
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
      BINARY_DIR "${TOLK_BRIDGE_X64_DIR}"
      CMAKE_GENERATOR "${CMAKE_GENERATOR}"
      CMAKE_GENERATOR_PLATFORM "x64"
      ${TOLK_BRIDGE_TOOLSET_ARGS}
      BUILD_COMMAND "${CMAKE_COMMAND}" --build "${TOLK_BRIDGE_X64_DIR}" --config Release
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS "${TOLK_BRIDGE_X64_EXE}"
    )
    tolk_bridge_embed_file(102 "${TOLK_BRIDGE_X64_EXE}")
    list(APPEND TOLK_BRIDGE_DEPENDS tolk_bridge_x64)
  endif()
  if(TOLK_BRIDGE_RC_CONTENT)
    set(TOLK_BRIDGE_RC "${CMAKE_BINARY_DIR}/tolk_bridge_resources.rc")
    file(WRITE "${TOLK_BRIDGE_RC}" "${TOLK_BRIDGE_RC_CONTENT}")
    target_sources(Tolk PRIVATE "${TOLK_BRIDGE_RC}")
    add_dependencies(Tolk ${TOLK_BRIDGE_DEPENDS})
  endif()
endif()
# Install rules
install(TARGETS Tolk
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES Tolk.h TolkVersion.h DESTINATION include)
# --- Post-build: assemble dist directory per configuration ---
# Use generator expressions for per-config output directories
set(TOLK_DIST_DIR_DEBUG "${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Debug")
set(TOLK_DIST_DIR_RELEASE "${CMAKE_BINARY_DIR}/dist/${TOLK_ARCH}-Release")
# Copy compiled Tolk.dll and Tolk.lib to dist after build (per config)
add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:Tolk>"
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_LINKER_FILE:Tolk>"
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/"
  COMMENT "Copying Tolk DLL/LIB to dist/${TOLK_ARCH}-$<CONFIG>"
)
# Copy architecture-specific libs (DLLs, ini files) to dist
file(GLOB TOLK_LIB_FILES "${TOLK_LIBS_DIR}/*")
# ARM64 and ARM64EC have no backend modules of their own for the 64-bit
# backends, so their distributions ship the x64 modules unchanged. The helper
# and Tolk.dll load them next to Tolk.dll, exactly like the x64 build does.
set(TOLK_EXTRA_LIB_FILES "")
if(TOLK_ARCH STREQUAL "arm64" OR TOLK_ARCH STREQUAL "arm64ec")
  file(GLOB TOLK_EXTRA_LIB_FILES "${CMAKE_SOURCE_DIR}/libs/x64/*")
  # NVDA uses its own native client on every architecture.
  list(REMOVE_ITEM TOLK_EXTRA_LIB_FILES "${CMAKE_SOURCE_DIR}/libs/x64/nvdaControllerClient64.dll")
endif()
add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${TOLK_LIB_FILES} ${TOLK_EXTRA_LIB_FILES}
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/"
  COMMENT "Copying libs/${TOLK_ARCH} dependencies to dist/${TOLK_ARCH}-$<CONFIG>"
)

# Python wrapper bytecode compilation
add_subdirectory(python)

# Copy script wrappers to separate language directories
add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/autoit"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/src/autoit/Tolk.au3"
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/autoit/"
  COMMENT "Copying AutoIt wrapper to dist/${TOLK_ARCH}-$<CONFIG>/autoit"
)

add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/purebasic"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/src/purebasic/Tolk.pb"
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/purebasic/"
  COMMENT "Copying PureBasic wrapper to dist/${TOLK_ARCH}-$<CONFIG>/purebasic"
)

add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/python"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/src/python/Tolk.py"
    "$<$<CONFIG:Debug>:${TOLK_DIST_DIR_DEBUG}>$<$<CONFIG:Release>:${TOLK_DIST_DIR_RELEASE}>/python/"
  COMMENT "Copying Python wrapper to dist/${TOLK_ARCH}-$<CONFIG>/python"
)
