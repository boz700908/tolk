cmake_minimum_required(VERSION 3.20)
project(TolkZdCloudBridge32 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TolkZDCloudBridge32 main.cpp)

target_compile_definitions(TolkZDCloudBridge32 PRIVATE UNICODE _UNICODE)

if(MSVC)
  target_compile_options(TolkZDCloudBridge32 PRIVATE /W4 /EHsc)
  target_compile_definitions(TolkZDCloudBridge32 PRIVATE _WIN32_WINNT=0x0601 WINVER=0x0601)
endif()

target_link_libraries(TolkZDCloudBridge32 PRIVATE Kernel32)

# Always emit the executable into the build root so the outer build can embed
# it at a deterministic path regardless of the configuration.
set_target_properties(TolkZDCloudBridge32 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(TolkZDCloudBridge32 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}")
endforeach()
