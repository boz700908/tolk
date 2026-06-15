set(TOLK_SOURCES
  Tolk.cpp
  ScreenReaderDriverBOY.cpp
  ScreenReaderDriverJAWS.cpp
  ScreenReaderDriverNVDA.cpp
  ScreenReaderDriverSA.cpp
  ScreenReaderDriverSAPI.cpp
  ScreenReaderDriverSNova.cpp
  ScreenReaderDriverWE.cpp
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
