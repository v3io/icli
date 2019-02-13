
cmake_minimum_required(VERSION 2.8.6)

# Enable folder grouping of projects in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (NOT CMAKE_BUILD_TYPE)
    message(WARNING "No build type selected, defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(icli)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
set(warning_flags "-Wall \
                  -Werror \
                  -Wsign-compare \
                  -Wclobbered \
                  -Wempty-body \
                  -Wignored-qualifiers \
                  -Wsign-compare \
                  -Wtype-limits \
                  -Wunused-but-set-parameter \
                  -Wconversion \
                  -Winit-self \
                  -Wno-unused-function")

set(CMAKE_CXX_FLAGS_DEBUG "${warning_flags} \
                          -O0 \
                          -ggdb \
                          -D_FORTIFY_SOURCE=2")

set(CMAKE_CXX_FLAGS_RELEASE "${warning_flags} \
                            -O2 \
                            -DNDEBUG \
                            -U_FORTIFY_SOURCE")

set(target libicli)

add_library(${target} STATIC icli.c)
target_include_directories(${target} PUBLIC .)

set(target cli)

add_executable(${target} examples/cli.c)
target_include_directories(${target} PUBLIC .)

target_link_libraries(${target}
                      libicli
                      edit
                      )
