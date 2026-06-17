cmake_minimum_required(VERSION 3.20)
project(Tolk VERSION 2.0.0 LANGUAGES C CXX)

# Modern C++17 standard with Windows 7 runtime compatibility
# C++17 features available, but generated binaries still compatible with Windows 7+
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Modern C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# MSVC: Enable modern compiler features, strict conformance, Windows 7 compatible
if(MSVC)
  add_compile_options(/permissive-)
  # Optimization flags
  add_compile_options(/Oi /Ot /GL)
  # Windows 7 compatibility (target subsystem version)
  add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)
endif()

# Options
option(TOLK_BUILD_JNI "Build with Java Native Interface support" ON)
option(TOLK_BUILD_DOTNET "Build .NET wrapper DLL" ON)
option(TOLK_BUILD_JAVA "Build Java JAR" ON)
option(TOLK_BUILD_DOCS "Build documentation" ON)

# Detect architecture for libs directory
if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
  set(TOLK_ARCH "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
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
