find_program(CARGO_EXECUTABLE cargo)
if (NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR "cargo not found; install Rust or configure without -DNGOSEN_RUST_CORE=ON")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(NGOSEN_CARGO_PROFILE_FLAG "")
    set(NGOSEN_CARGO_PROFILE_DIR debug)
else()
    set(NGOSEN_CARGO_PROFILE_FLAG --release)
    set(NGOSEN_CARGO_PROFILE_DIR release)
endif()

set(NGOSEN_CARGO_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target" CACHE INTERNAL "")
set(NGOSEN_BAMBOO_LIB "${NGOSEN_CARGO_TARGET_DIR}/${NGOSEN_CARGO_PROFILE_DIR}/libngosen_bamboo.a")

file(GLOB_RECURSE NGOSEN_BAMBOO_SRCS src/*.rs)

# Cargo reads the vendored-sources config from the working directory, not the manifest's.
add_custom_command(OUTPUT "${NGOSEN_BAMBOO_LIB}"
    DEPENDS ${NGOSEN_BAMBOO_SRCS} Cargo.toml Cargo.lock .cargo/config.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${NGOSEN_CARGO_TARGET_DIR}
            ${CARGO_EXECUTABLE} build ${NGOSEN_CARGO_PROFILE_FLAG} --offline --locked --lib
    VERBATIM)
add_custom_target(ngosen-bamboo DEPENDS "${NGOSEN_BAMBOO_LIB}")

add_library(Bamboo::Core STATIC IMPORTED GLOBAL)
add_dependencies(Bamboo::Core ngosen-bamboo)
# The libraries rustc reports for a staticlib (--print native-static-libs).
set_target_properties(Bamboo::Core PROPERTIES
    IMPORTED_LOCATION "${NGOSEN_BAMBOO_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    INTERFACE_LINK_LIBRARIES "gcc_s;util;rt;pthread;m;dl")
