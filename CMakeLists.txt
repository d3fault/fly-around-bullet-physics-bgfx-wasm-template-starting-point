cmake_minimum_required(VERSION 3.10)

# If the EMSDK environment variable is set, configure the toolchain file. this has to be done before the project() line below
if(DEFINED ENV{EMSDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "Emscripten toolchain file")
    message(STATUS "WebAssembly build enabled")
    message(STATUS "CMAKE_TOOLCHAIN_FILE='${CMAKE_TOOLCHAIN_FILE}'")
    if(NOT BULLET_SRC_DIR)
        message(FATAL_ERROR "Because you're targetting WebAssembly, the BULLET_SRC_DIR variable must be manually set. Use -DBULLET_SRC_DIR=/path/to/bullet3 when running CMake.")
    endif()
    if(NOT BULLET_BUILD_DIR)
        message(FATAL_ERROR "Because you're targetting WebAssembly, the BULLET_BUILD_DIR variable must be manually set. Use -DBULLET_BUILD_DIR=/path/to/bullet3/buildwasm when running CMake.")
    endif()
else()
    message(STATUS "WebAssembly build NOT enabled. EMSDK environment variable is not set. Falling back to native build. If you want to compile for WebAssembly, first `source emsdk_env.sh` in the emsdk directory and then do `mkdir buildwasm && cd buildwasm && emcmake cmake .. && emmake make`")
endif()

project(bullet-physics-bgfx-glfw-wasm-template-starting-point)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(BGFX_DIR ${CMAKE_SOURCE_DIR}/bgfx)

if(DEFINED ENV{EMSDK})
    if(WIN32)
        message(FATAL_ERROR "This script is not supported on Windows.")
    endif()
    #CMake sucks. TODO: windows portability `pwd`

    #resolve BULLET_SRC_DIR
    if(NOT IS_ABSOLUTE ${BULLET_SRC_DIR})
        execute_process(
            COMMAND pwd
            OUTPUT_VARIABLE CWD
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(BULLET_SRC_DIR "${CWD}/${BULLET_SRC_DIR}")
    endif()
    get_filename_component(BULLET_SRC_DIR "${BULLET_SRC_DIR}" ABSOLUTE)
    #resolve BULLET_BUILD_DIR
    if(NOT IS_ABSOLUTE ${BULLET_BUILD_DIR})
        execute_process(
            COMMAND pwd
            OUTPUT_VARIABLE CWD
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        set(BULLET_BUILD_DIR "${CWD}/${BULLET_BUILD_DIR}")
    endif()
    get_filename_component(BULLET_BUILD_DIR "${BULLET_BUILD_DIR}" ABSOLUTE)

    #find_path(BULLET_SRC_DIR_AFTER_FIND_PATH NAMES btBulletDynamicsCommon.h PATHS ${BULLET_SRC_DIR}/src)

    #if(BULLET_SRC_DIR_AFTER_FIND_PATH)
        #include_directories(${BULLET_SRC_DIR_AFTER_FIND_PATH})
    if(EXISTS "${BULLET_SRC_DIR}/src")
        include_directories(${BULLET_SRC_DIR}/src)
    else()
        message(FATAL_ERROR "Could not find bullet include btBulletDynamicsCommon.h in ${BULLET_SRC_DIR}/src")
    endif()

    #find_library(BULLET_LINEAR_MATH_LIB NAMES libLinearMath.a PATHS ${BULLET_BUILD_DIR}/src/LinearMath)
    set(BULLET_LINEAR_MATH_LIB "${BULLET_BUILD_DIR}/src/LinearMath/libLinearMath.a")
    #find_library(BULLET_COLLISION_LIB NAMES libBulletCollision.a PATHS ${BULLET_BUILD_DIR}/src/BulletCollision)
    set(BULLET_COLLISION_LIB "${BULLET_BUILD_DIR}/src/BulletCollision/libBulletCollision.a")
    #find_library(BULLET_DYNAMICS_LIB NAMES libBulletDynamics.a PATHS ${BULLET_BUILD_DIR}/src/BulletDynamics)
    set(BULLET_DYNAMICS_LIB "${BULLET_BUILD_DIR}/src/BulletDynamics/libBulletDynamics.a")

    #if(EXISTS BULLET_LINEAR_MATH_LIB AND EXISTS BULLET_COLLISION_LIB AND EXISTS BULLET_DYNAMICS_LIB)
    #    message(STATUS "Found all bullet libs!")
    #else()
    #    message(FATAL_ERROR "Could not find Bullet libraries. Check paths. Did you compile them? ${BULLET_LINEAR_MATH_LIB} , ${BULLET_COLLISION_LIB} , ${BULLET_DYNAMICS_LIB}")
    #endif()
else()
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Bullet COMPONENTS BulletCollision BulletDynamics REQUIRED)
    include_directories(${BULLET_INCLUDE_DIR})
endif()

include_directories(${BGFX_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/bx/include)
include_directories(${CMAKE_SOURCE_DIR}/bimg/include)

add_definitions(-DBX_CONFIG_DEBUG)

add_executable(bullet-physics-bgfx-glfw-wasm-template-starting-point main.cpp)

file(INSTALL DESTINATION ${CMAKE_BINARY_DIR}
    TYPE FILE
    FILES ${BGFX_DIR}/examples/runtime/shaders/glsl/vs_cubes.bin
    ${BGFX_DIR}/examples/runtime/shaders/glsl/fs_cubes.bin)

if(DEFINED ENV{EMSDK})
    set(CMAKE_CXX_FLAGS "-s MIN_WEBGL_VERSION=2 \
        -s MAX_WEBGL_VERSION=2 \
        -s EXCEPTION_DEBUG \
        -fexceptions \
        --preload-file vs_cubes.bin \
        --preload-file fs_cubes.bin \
        --bind \
        --use-preload-plugins \
        -Wall \
        -Wextra \
        -Werror=return-type \
        -s ASSERTIONS=1 \
        -w \
        -g4 \
        -s DISABLE_EXCEPTION_CATCHING=0 \
        -Os \
        -s USE_GLFW=3 \
        --no-heap-copy \
        -s GL_ENABLE_GET_PROC_ADDRESS \
        -static \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s NO_EXIT_RUNTIME=1")

    target_link_libraries(bullet-physics-bgfx-glfw-wasm-template-starting-point PRIVATE
        ${BGFX_DIR}/.build/wasm/bin/bgfxDebug.bc
        ${BGFX_DIR}/.build/wasm/bin/bxDebug.bc
        ${BGFX_DIR}/.build/wasm/bin/bimgDebug.bc
        ${BULLET_LINEAR_MATH_LIB} ${BULLET_COLLISION_LIB} ${BULLET_DYNAMICS_LIB})

    file(INSTALL DESTINATION ${CMAKE_BINARY_DIR}
        TYPE FILE
        FILES ${CMAKE_SOURCE_DIR}/index.html)
else() # Native Linux/X11
    target_link_libraries(bullet-physics-bgfx-glfw-wasm-template-starting-point PRIVATE
        glfw
        OpenGL::GL
        ${BGFX_DIR}/.build/linux64_gcc/bin/libbgfx-shared-libDebug.so
	${BULLET_LIBRARIES})
endif()
