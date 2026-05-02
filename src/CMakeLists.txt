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
  find_package(JNI REQUIRED)
  list(APPEND TOLK_SOURCES TolkJNI.cpp)
endif()

# DLL target
add_library(Tolk SHARED ${TOLK_SOURCES} ${TOLK_HEADERS} Tolk.rc)

target_compile_definitions(Tolk PRIVATE
  _EXPORTING
  UNICODE
  _UNICODE
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

# MSVC-specific settings
if(MSVC)
  target_compile_options(Tolk PRIVATE /W4 /O2 /EHsc /Gw)
endif()

# Install rules
install(TARGETS Tolk
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES Tolk.h TolkVersion.h DESTINATION include)
