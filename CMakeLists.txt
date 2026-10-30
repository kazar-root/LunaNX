cmake_minimum_required(VERSION 3.18)

if (NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO n'est pas défini. Installe devkitPro puis exporte DEVKITPRO=/opt/devkitpro.")
endif ()

set(DEVKITPRO $ENV{DEVKITPRO})
set(CMAKE_TOOLCHAIN_FILE ${DEVKITPRO}/cmake/Switch.cmake CACHE FILEPATH "devkitPro Switch toolchain")

project(LunaNX LANGUAGES C CXX)

set(PLATFORM_SWITCH ON)
# Une seule application deko3d : décodage NVDEC sans copie ou envoi YUV logiciel,
# le choix se fait à l'exécution dans les réglages.
set(USE_GLFW OFF CACHE BOOL "" FORCE)
set(USE_DEKO3D ON CACHE BOOL "" FORCE)
set(BOREALIS_USE_DEKO3D ON CACHE BOOL "" FORCE)
set(USE_GLES2 OFF CACHE BOOL "" FORCE)
set(USE_SDL2 OFF)
set(USE_GLES3 OFF CACHE BOOL "")
set(BRLS_UNITY_BUILD OFF CACHE BOOL "")
set(FMT_OS OFF CACHE BOOL "")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Aucun chemin local (nom d'utilisateur, dossiers) ne doit finir dans le binaire via
# __FILE__ ou les infos de débogage : on les réécrit en chemins relatifs.
add_compile_options(
    -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
    -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)

add_subdirectory(extern/borealis/library)
add_subdirectory(extern/libpeer)

target_include_directories(borealis PUBLIC ${DEVKITPRO}/portlibs/switch/include)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS app/src/*.cpp app/src/*.c)

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    extern/borealis/library/lib/platforms/switch/switch_wrapper.c)

target_include_directories(${PROJECT_NAME} PRIVATE
    app/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/glfw/deps
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/libpeer/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/libpeer/third_party/mbedtls/include)

# Les dépendances de libpeer compilées dans l'arbre passent avant les portlibs du même
# nom : mélanger deux versions de mbedTLS casse le TLS.
target_link_directories(${PROJECT_NAME} BEFORE PRIVATE
    ${CMAKE_BINARY_DIR}/dist/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg/lib)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLATFORM_SWITCH __SWITCH__ HAS_SOCKLEN_T=1
    USE_DK_RENDERER BOREALIS_USE_DEKO3D LUNANX_ENABLE_NVDEC)

# Shaders deko3d (interface Borealis + rendu vidéo), compilés avec uam.
find_program(NX_UAM_EXE NAMES uam HINTS "${DEVKITPRO}/tools/bin")
if (NOT NX_UAM_EXE)
    message(FATAL_ERROR "uam (compilateur de shaders deko3d) est introuvable")
endif ()
set(SHADER_OUT "${CMAKE_BINARY_DIR}/resources/shaders")
set(BOREALIS_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library/lib/extern/nanovg/deko3d/shaders")
file(MAKE_DIRECTORY "${SHADER_OUT}")
foreach (shader IN ITEMS "vert;fill_vsh;${BOREALIS_SHADERS}/fill_vsh.glsl"
                         "frag;fill_aa_fsh;${BOREALIS_SHADERS}/fill_aa_fsh.glsl"
                         "frag;fill_fsh;${BOREALIS_SHADERS}/fill_fsh.glsl"
                         "vert;lunanx_video_vsh;${CMAKE_CURRENT_SOURCE_DIR}/app/src/stream/deko3d/basic_vsh.glsl"
                         "frag;lunanx_video_fsh;${CMAKE_CURRENT_SOURCE_DIR}/app/src/stream/deko3d/texture_fsh.glsl")
    list(GET shader 0 stage)
    list(GET shader 1 name)
    list(GET shader 2 source)
    execute_process(COMMAND ${NX_UAM_EXE} -s ${stage} -o "${SHADER_OUT}/${name}.dksh" "${source}"
                    COMMAND_ERROR_IS_FATAL ANY)
endforeach ()

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections)

target_link_libraries(${PROJECT_NAME} PRIVATE
    borealis
    PkgConfig::CURL
    PkgConfig::JANSSON
    peer
    -lavformat -lavcodec -lavutil -lswresample
    z bz2)

set(PROJECT_AUTHOR "KazaR-RooT")
set(PROJECT_VERSION_STRING "1.0")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon.jpg)
set(PROJECT_ROMFS_DIR ${CMAKE_BINARY_DIR}/romfs)

add_custom_target(${PROJECT_NAME}.nro
    DEPENDS ${PROJECT_NAME}
    COMMAND ${NX_NACPTOOL_EXE} --create "LunaNX" "${PROJECT_AUTHOR}" "${PROJECT_VERSION_STRING}"
        ${PROJECT_NAME}.nacp
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_ROMFS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_ROMFS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n ${PROJECT_ROMFS_DIR}/i18n
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon ${PROJECT_ROMFS_DIR}/icon
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/img ${PROJECT_ROMFS_DIR}/img
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/material ${PROJECT_ROMFS_DIR}/material
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUT} ${PROJECT_ROMFS_DIR}/shaders
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
        --icon=${PROJECT_ICON}
        --nacp=${PROJECT_NAME}.nacp
        --romfsdir=${PROJECT_ROMFS_DIR})
