
cmake_minimum_required(VERSION 2.8.6)

# Enable folder grouping of projects in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (NOT CMAKE_BUILD_TYPE)
    message(WARNING "No build type selected, defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(icli)

if (INCLUDE_PATH)
    include_directories(BEFORE ${INCLUDE_PATH})
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -std=c99")
set(warning_flags "-Wall \
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

set(CMAKE_C_FLAGS_DEBUG "${warning_flags} \
                          -O0 \
                          -ggdb \
                          -D_FORTIFY_SOURCE=2")

set(CMAKE_C_FLAGS_RELEASE "${warning_flags} \
                            -O2 \
                            -DNDEBUG \
                            -U_FORTIFY_SOURCE")

set(target icli)

add_library(${target} STATIC icli.c)
target_include_directories(${target} PUBLIC .)

set(target cli)

add_executable(${target} EXCLUDE_FROM_ALL examples/cli.c)
target_include_directories(${target} PUBLIC .)

target_link_libraries(${target}
                      icli
                      edit
                      )

find_package(Doxygen)

if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
                          ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                      COMMENT "Generating API documentation with Doxygen" VERBATIM
                     )
endif(DOXYGEN_FOUND)
