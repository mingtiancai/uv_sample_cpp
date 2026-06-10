cmake_minimum_required(VERSION 3.16)

project(libuv_sample_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libuv locations are provided externally (e.g. via build.sh / -D flags or
# environment variables). No machine-specific paths are hardcoded here.
set(LIBUV_INSTALL_DIR "$ENV{LIBUV_INSTALL_DIR}" CACHE PATH "Path to the libuv install prefix")
set(LIBUV_SOURCE_DIR "$ENV{LIBUV_SOURCE_DIR}" CACHE PATH "Path to the libuv source tree")
set(LIBUV_BUILD_DIR "$ENV{LIBUV_BUILD_DIR}" CACHE PATH "Path to the libuv build tree")

if(EXISTS "${LIBUV_INSTALL_DIR}/lib/cmake/libuv/libuvConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${LIBUV_INSTALL_DIR}")
    find_package(libuv CONFIG REQUIRED)
else()
    find_path(LIBUV_INCLUDE_DIR uv.h
        HINTS
            "${LIBUV_INSTALL_DIR}/include"
            "${LIBUV_SOURCE_DIR}/include"
    )

    find_library(LIBUV_LIBRARY
        NAMES uv libuv
        HINTS
            "${LIBUV_INSTALL_DIR}/lib"
            "${LIBUV_BUILD_DIR}"
    )

    if(NOT LIBUV_INCLUDE_DIR OR NOT LIBUV_LIBRARY)
        message(FATAL_ERROR "Could not find libuv. Set LIBUV_INSTALL_DIR, LIBUV_SOURCE_DIR, or LIBUV_BUILD_DIR.")
    endif()

    add_library(libuv::uv UNKNOWN IMPORTED)
    set_target_properties(libuv::uv PROPERTIES
        IMPORTED_LOCATION "${LIBUV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "pthread;dl;rt;m"
    )
endif()


add_executable(hello_world
    src/hello_world.cpp
)

target_compile_definitions(hello_world PRIVATE _GNU_SOURCE)
target_link_libraries(hello_world PRIVATE libuv::uv)

add_executable(watcher_cross_stop
    src/watcher_cross_stop.cpp
)

target_compile_definitions(watcher_cross_stop PRIVATE _GNU_SOURCE)
target_link_libraries(watcher_cross_stop PRIVATE libuv::uv)
