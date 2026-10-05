cmake_minimum_required(VERSION 3.20)
project(TolkBridge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The helper is only ever built as x86 or x64: it hosts the drivers a Tolk.dll
# of another architecture cannot load in-process. It reuses the very same
# driver sources as Tolk.dll, so the two can never drift apart.
add_executable(TolkBridge
  main.cpp
  ../ScreenReaderDriverNVDA.cpp
  ../ScreenReaderDriverJAWS.cpp
  ../ScreenReaderDriverWE.cpp
  ../ScreenReaderDriverSA.cpp
  ../ScreenReaderDriverSNova.cpp
  ../ScreenReaderDriverZDSR.cpp
  ../ScreenReaderDriverBOY.cpp
  ../ScreenReaderDriverZDCloud.cpp
  ../ScreenReaderDriverZT.cpp
  ../ScreenReaderDriverPCTalker.cpp
  ../ScreenReaderDriverSenseReader.cpp
  ../fsapi.c
  ../wineyes.c
  ../zt.c
)

target_include_directories(TolkBridge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(TolkBridge PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0601
  WINVER=0x0601
)
target_link_libraries(TolkBridge PRIVATE User32 Ole32 OleAut32)

if(MSVC)
  target_compile_options(TolkBridge PRIVATE /W3 /EHsc /permissive-)
  target_compile_options(TolkBridge PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# Always emit the executable into the build root so the outer build can embed
# it at a deterministic path regardless of the configuration.
set_target_properties(TolkBridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(TolkBridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}")
endforeach()
