set(TOLK_SOURCES
  Tolk.cpp
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
  ScreenReaderDriver.h
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
# --- ZDCloud 32-bit bridge for 64-bit builds ---
# A 64-bit process cannot load the 32-bit ZDCloudAPI.dll. A small 32-bit bridge
# is therefore built separately (forced to Win32) and embedded, together with
# ZDCloudAPI.dll, into Tolk.dll as resources. At run time they are extracted to
# a per-user cache directory, so no extra files are shipped next to Tolk.dll.
if(TOLK_ARCH STREQUAL "x64" AND CMAKE_GENERATOR MATCHES "Visual Studio")
  include(ExternalProject)
  set(TOLK_ZDCLOUD_BRIDGE_DIR "${CMAKE_BINARY_DIR}/zdcloud_bridge32")
  set(TOLK_ZDCLOUD_BRIDGE_EXE "${TOLK_ZDCLOUD_BRIDGE_DIR}/TolkZDCloudBridge32.exe")
  set(TOLK_ZDCLOUD_API_DLL "${CMAKE_SOURCE_DIR}/libs/x86/ZDCloudAPI.dll")
  set(TOLK_ZDCLOUD_BRIDGE_ARGS
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_GENERATOR_PLATFORM "Win32"
  )
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND TOLK_ZDCLOUD_BRIDGE_ARGS CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  ExternalProject_Add(tolk_zdcloud_bridge32
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zdcloud_bridge32"
    BINARY_DIR "${TOLK_ZDCLOUD_BRIDGE_DIR}"
    ${TOLK_ZDCLOUD_BRIDGE_ARGS}
    BUILD_COMMAND "${CMAKE_COMMAND}" --build "${TOLK_ZDCLOUD_BRIDGE_DIR}" --config Release
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${TOLK_ZDCLOUD_BRIDGE_EXE}"
  )
  add_dependencies(Tolk tolk_zdcloud_bridge32)
  file(TO_NATIVE_PATH "${TOLK_ZDCLOUD_BRIDGE_EXE}" TOLK_ZDCLOUD_BRIDGE_EXE_NATIVE)
  file(TO_NATIVE_PATH "${TOLK_ZDCLOUD_API_DLL}" TOLK_ZDCLOUD_API_DLL_NATIVE)
  # rc.exe treats backslashes as escape characters, so double them.
  string(REPLACE "\\" "\\\\" TOLK_ZDCLOUD_BRIDGE_EXE_NATIVE "${TOLK_ZDCLOUD_BRIDGE_EXE_NATIVE}")
  string(REPLACE "\\" "\\\\" TOLK_ZDCLOUD_API_DLL_NATIVE "${TOLK_ZDCLOUD_API_DLL_NATIVE}")
  set(TOLK_ZDCLOUD_RC "${CMAKE_BINARY_DIR}/zdcloud_resources.rc")
  file(WRITE "${TOLK_ZDCLOUD_RC}"
    "101 RCDATA \"${TOLK_ZDCLOUD_BRIDGE_EXE_NATIVE}\"\n"
    "102 RCDATA \"${TOLK_ZDCLOUD_API_DLL_NATIVE}\"\n")
  target_sources(Tolk PRIVATE "${TOLK_ZDCLOUD_RC}")
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
add_custom_command(TARGET Tolk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${TOLK_LIB_FILES}
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
