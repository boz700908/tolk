# Wrapper payloads that are compiled as part of a normal build.
#
# The GameMaker binding needs a tiny UTF-8/UTF-16 shim because GameMaker passes
# UTF-8 strings while Tolk speaks UTF-16. Building it here keeps it validated on
# every architecture and lets a release package ship it prebuilt; the source is
# still in contrib so it can be rebuilt for other architectures.

add_library(TolkGml SHARED "${CMAKE_SOURCE_DIR}/contrib/game-engines/gamemaker/source/tolk_gml.c")
target_include_directories(TolkGml PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(TolkGml PRIVATE Tolk)
set_target_properties(TolkGml PROPERTIES
  OUTPUT_NAME "TolkGml"
  FOLDER "wrappers"
)