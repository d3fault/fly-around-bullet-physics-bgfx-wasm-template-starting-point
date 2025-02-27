cmake_minimum_required(VERSION 3.10)

# If the EMSDK environment variable is set, configure the toolchain file. this has to be done before the project() line below
if(DEFINED ENV{EMSDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "Emscripten toolchain file")
    message(STATUS "WebAssembly build enabled")
    message(STATUS "CMAKE_TOOLCHAIN_FILE='${CMAKE_TOOLCHAIN_FILE}'")
else()
    message(STATUS "WebAssembly build NOT enabled. EMSDK environment variable is not set. Falling back to native build. If you want to compile for WebAssembly, first `source emsdk_env.sh` in the emsdk directory and then do `mkdir buildwasm && cd buildwasm && emcmake cmake .. && emmake make`")
endif()

project(fly-around-bullet-physics-bgfx-wasm-template-starting-point)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(BGFX_DIR ${CMAKE_SOURCE_DIR}/bgfx)

set(BGFX_LINUX64_BUILD_DIR ${BGFX_DIR}/.build/linux64_gcc)
if(DEFINED ENV{EMSDK})
    set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/wasm)
else()
    set(BGFX_BUILD_DIR ${BGFX_LINUX64_BUILD_DIR})
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Bullet COMPONENTS BulletCollision BulletDynamics REQUIRED)
    include_directories(${BULLET_INCLUDE_DIR})
    find_package(Boost REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(JPEG REQUIRED)
endif()

set(BGFX_SHADERC ${BGFX_LINUX64_BUILD_DIR}/bin/shadercRelease CACHE PATH "Path to Shaderc compiler") #TODO not very portable, but wasm needs to use the host one?

# Function to compile a single shader
function(compile_shader INPUT_FILE OUTPUT_FILE TYPE VARYING_FILE)
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND "${BGFX_SHADERC}"
            -f "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_FILE}"  # Input shader source
            -o "${OUTPUT_FILE}"  # Output compiled shader
            --type "${TYPE}"  # Shader type (determined in loop)
            --varyingdef "${VARYING_FILE}"  # Shader-specific varying.def.sc
            --platform linux  # Change depending on your platform
            --verbose
            -i "${BGFX_DIR}/src"
        DEPENDS "${INPUT_FILE}" "${VARYING_FILE}"  # Ensure recompilation if varying file changes
        COMMENT "Compiling shader: ${INPUT_FILE} (${TYPE}) using varying file: ${VARYING_FILE}"
    )

    message(STATUS "Compiling ${INPUT_FILE} (${TYPE}) with varying: ${VARYING_FILE}")
endfunction()

# Shader list in pairs: (SHADER_FILE VARYING_FILE)
set(SHADERS
    "vs_overlay.sc" "${BGFX_DIR}/src/varying.def.sc"
    "fs_overlay.sc" "${BGFX_DIR}/src/varying.def.sc"
    "vs_skybox.sc"  "${CMAKE_CURRENT_SOURCE_DIR}/varying.skybox.def.sc"
    "fs_skybox.sc"  "${CMAKE_CURRENT_SOURCE_DIR}/varying.skybox.def.sc"
)

set(COMPILED_SHADERS "")  # List to track compiled shader outputs

# Ensure SHADERS list has an even number of elements (pairs)
list(LENGTH SHADERS SHADER_COUNT)
# Iterate over SHADERS in pairs
math(EXPR MAX_INDEX "${SHADER_COUNT} - 1")  # Ensure INDEX+1 is valid
foreach(INDEX RANGE 0 ${MAX_INDEX} 2)
    # Extract the key-value pair as a list (CMake treats space-separated strings as a single item!)
    list(SUBLIST SHADERS ${INDEX} 2 SHADER_PAIR)

    # Ensure it's a valid key-value pair
    list(LENGTH SHADER_PAIR PAIR_LEN)
    if(NOT PAIR_LEN EQUAL 2)
        message(FATAL_ERROR "Invalid shader entry at index ${INDEX}: ${SHADER_PAIR}")
    endif()

    list(GET SHADER_PAIR 0 SHADER_FILE)
    list(GET SHADER_PAIR 1 VARYING_FILE)

    # Compute output filename
    get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME_WE)
    set(OUTPUT_SHADER "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.bin")

    # Detect shader type from filename
    if(SHADER_NAME MATCHES "^vs_")
        set(SHADER_TYPE "vertex")
    elseif(SHADER_NAME MATCHES "^fs_")
        set(SHADER_TYPE "fragment")
    else()
        message(FATAL_ERROR "Unknown shader type for ${SHADER_NAME}")
    endif()

    # Compile the shader
    compile_shader("${SHADER_FILE}" "${OUTPUT_SHADER}" "${SHADER_TYPE}" "${VARYING_FILE}")

    # Track compiled outputs
    list(APPEND COMPILED_SHADERS "${OUTPUT_SHADER}")
endforeach()

# Ensure shaders are built before the main target
add_custom_target(compile_shaders DEPENDS ${COMPILED_SHADERS})

include_directories(${BGFX_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/bx/include)
include_directories(${CMAKE_SOURCE_DIR}/bimg/include)

add_definitions(-DBX_CONFIG_DEBUG)

add_executable(fly-around-bullet-physics-bgfx-wasm-template-starting-point main.cpp bgfx2doverlaytext_dynamicallysized.cpp bgfx2doverlaytext_dynamicallysized.h bgfx2doverlaytext_staticallysized.cpp bgfx2doverlaytext_staticallysized.h lolreadfile.h lolreadfile.cpp skybox.cpp skybox.h my_bgfx_view_id_constants.h)

file(INSTALL DESTINATION ${CMAKE_BINARY_DIR}
    TYPE FILE
    FILES ${BGFX_DIR}/examples/runtime/shaders/glsl/vs_cubes.bin
    ${BGFX_DIR}/examples/runtime/shaders/glsl/fs_cubes.bin
    Vegur-Regular.ttf
    skybox.jpg
    )

if(DEFINED ENV{EMSDK})
    set(CMAKE_CXX_FLAGS "-s MIN_WEBGL_VERSION=2 \
        -s MAX_WEBGL_VERSION=2 \
        -s EXCEPTION_DEBUG \
        -fexceptions \
        --preload-file vs_cubes.bin \
        --preload-file fs_cubes.bin \
        --preload-file vs_overlay.bin \
        --preload-file fs_overlay.bin \
        --preload-file Vegur-Regular.ttf \
        --preload-file vs_skybox.bin \
        --preload-file fs_skybox.bin \
        --preload-file skybox.jpg \
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
        -s USE_BULLET=1 \
        -s USE_BOOST_HEADERS=1 \
        -s USE_FREETYPE=1 \
        -s USE_LIBJPEG=1 \
        --no-heap-copy \
        -s GL_ENABLE_GET_PROC_ADDRESS \
        -static \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s STACK_SIZE=8MB \
        -s NO_EXIT_RUNTIME=1")

    target_link_libraries(fly-around-bullet-physics-bgfx-wasm-template-starting-point PRIVATE
        ${BGFX_DIR}/.build/wasm/bin/bgfxDebug.bc
        ${BGFX_DIR}/.build/wasm/bin/bxDebug.bc
        ${BGFX_DIR}/.build/wasm/bin/bimgDebug.bc)

    file(INSTALL DESTINATION ${CMAKE_BINARY_DIR}
        TYPE FILE
        FILES ${CMAKE_SOURCE_DIR}/index.html)
else() # Native Linux/X11
    target_link_libraries(fly-around-bullet-physics-bgfx-wasm-template-starting-point PRIVATE
        glfw
        OpenGL::GL
        ${BGFX_DIR}/.build/linux64_gcc/bin/libbgfx-shared-libDebug.so
        ${BULLET_LIBRARIES}
        Freetype::Freetype
        JPEG::JPEG
        )
endif()
add_dependencies(fly-around-bullet-physics-bgfx-wasm-template-starting-point compile_shaders)
