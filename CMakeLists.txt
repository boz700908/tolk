cmake_minimum_required(VERSION 3.20)
project(Tolk VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Options
option(TOLK_BUILD_JNI "Build with Java Native Interface support" ON)
option(TOLK_BUILD_DOTNET "Build .NET wrapper DLL" ON)
option(TOLK_BUILD_JAVA "Build Java JAR" ON)
option(TOLK_BUILD_DOCS "Build documentation" ON)

# Detect architecture for libs directory
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TOLK_ARCH "x64")
else()
  set(TOLK_ARCH "x86")
endif()
set(TOLK_LIBS_DIR "${CMAKE_SOURCE_DIR}/libs/${TOLK_ARCH}")

# Core C++ DLL
add_subdirectory(src)

# .NET wrapper
if(TOLK_BUILD_DOTNET)
  add_subdirectory(src/dotnet)
endif()

# Java JAR
if(TOLK_BUILD_JAVA)
  add_subdirectory(src/java)
endif()

# Documentation
if(TOLK_BUILD_DOCS)
  add_subdirectory(docs)
endif()
