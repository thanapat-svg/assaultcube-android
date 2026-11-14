cmake_minimum_required(VERSION 3.18)
project(assaultcube_android C CXX)

# Third-party sources are fetched by scripts/fetch-deps.sh into app/jni/deps
set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)
get_filename_component(ACROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)
set(ACSRC ${ACROOT}/source/src)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# 16 KB page alignment for Android 15+ devices
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

# ---------------------------------------------------------------- SDL2 (shared, loaded by SDLActivity)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL SDL)

# ---------------------------------------------------------------- SDL2_image (static, stb backend: PNG/JPG/BMP/TGA)
file(GLOB SDLIMAGE_SRC ${DEPS}/SDL_image/src/IMG*.c)
list(FILTER SDLIMAGE_SRC EXCLUDE REGEX "IMG_WIC\\.c$")
add_library(SDL2_image_static STATIC ${SDLIMAGE_SRC})
target_include_directories(SDL2_image_static PUBLIC ${DEPS}/SDL_image/include)
target_compile_definitions(SDL2_image_static PRIVATE USE_STBIMAGE LOAD_PNG LOAD_JPG LOAD_BMP LOAD_TGA)
target_link_libraries(SDL2_image_static PRIVATE SDL2)

# ---------------------------------------------------------------- ogg + vorbis (static)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/ogg ogg)

set(VORBIS_DIR ${DEPS}/vorbis/lib)
add_library(vorbis_static STATIC
    ${VORBIS_DIR}/mdct.c ${VORBIS_DIR}/smallft.c ${VORBIS_DIR}/block.c ${VORBIS_DIR}/envelope.c
    ${VORBIS_DIR}/window.c ${VORBIS_DIR}/lsp.c ${VORBIS_DIR}/lpc.c ${VORBIS_DIR}/analysis.c
    ${VORBIS_DIR}/synthesis.c ${VORBIS_DIR}/psy.c ${VORBIS_DIR}/info.c ${VORBIS_DIR}/floor1.c
    ${VORBIS_DIR}/floor0.c ${VORBIS_DIR}/res0.c ${VORBIS_DIR}/mapping0.c ${VORBIS_DIR}/registry.c
    ${VORBIS_DIR}/codebook.c ${VORBIS_DIR}/sharedbook.c ${VORBIS_DIR}/lookup.c ${VORBIS_DIR}/bitrate.c
    ${VORBIS_DIR}/vorbisfile.c)
target_include_directories(vorbis_static PUBLIC ${DEPS}/vorbis/include PRIVATE ${VORBIS_DIR})
target_link_libraries(vorbis_static PUBLIC ogg)
target_compile_options(vorbis_static PRIVATE -w)

# ---------------------------------------------------------------- OpenAL Soft (static, OpenSL ES backend)
set(LIBTYPE STATIC CACHE STRING "" FORCE)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
set(ALSOFT_REQUIRE_OPENSL ON CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/openal-soft openal-soft)

# ---------------------------------------------------------------- gl4es (desktop OpenGL 1.x/2.x on top of GLES2)
set(GL4ES ${DEPS}/gl4es)
file(GLOB GL4ES_SRC ${GL4ES}/src/gl/*.c ${GL4ES}/src/gl/wrap/*.c ${GL4ES}/src/gl/math/*.c)
list(APPEND GL4ES_SRC ${GL4ES}/src/glx/hardext.c ${GL4ES}/src/glx/glx.c ${GL4ES}/src/glx/lookup.c
                      ${GL4ES}/src/glx/gbm.c ${GL4ES}/src/glx/streaming.c)
add_library(gl4es STATIC ${GL4ES_SRC})
target_include_directories(gl4es PRIVATE ${GL4ES}/include)
target_compile_definitions(gl4es PRIVATE NOX11 NO_GBM DEFAULT_ES=2 STATICLIB)
target_compile_options(gl4es PRIVATE -std=gnu99 -funwind-tables -O3 -fvisibility=hidden
    -include ${GL4ES}/include/android_debug.h -w)
target_link_libraries(gl4es PRIVATE log dl)

# ---------------------------------------------------------------- ENet
set(ENET ${ACROOT}/source/enet)
add_library(enet STATIC ${ENET}/callbacks.c ${ENET}/compress.c ${ENET}/host.c ${ENET}/list.c
                        ${ENET}/packet.c ${ENET}/peer.c ${ENET}/protocol.c ${ENET}/unix.c)
target_include_directories(enet PUBLIC ${ENET}/include)
target_compile_definitions(enet PRIVATE HAS_FCNTL=1 HAS_POLL=1 HAS_INET_PTON=1 HAS_INET_NTOP=1
                                        HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
target_compile_options(enet PRIVATE -w)

# ---------------------------------------------------------------- AssaultCube client (libmain.so)
set(AC_SOURCES
    crypto audiomanager autodownload client clientgame clients2c command console docs editing
    entities http log main menus oggstream openal packetqueue physics protocol rendercubes
    rendergl renderhud rendermodel renderparticles rendertext rndmap scoreboard serverms server
    serverbrowser shadow soundlocation soundscheduler stream texture tools water weapon wizard
    world worldio worldlight worldocull worldrender zip
    bot/bot bot/botmanager bot/bot_ai bot/bot_util bot/bot_waypoint bot/ac_bot bot/ac_bot_ai
    touch android_main)
set(AC_FILES "")
foreach(f ${AC_SOURCES})
    list(APPEND AC_FILES ${ACSRC}/${f}.cpp)
endforeach()

add_library(main SHARED ${AC_FILES})
target_include_directories(main PRIVATE ${ACSRC} ${ACSRC}/bot ${DEPS}/openal-soft/include)
# AC's bundled headers (GL/glext.h) go last so they never shadow the real SDL/OpenAL headers
target_compile_options(main PRIVATE -idirafter ${ACROOT}/source/include
    -fsigned-char -fomit-frame-pointer -Wno-everything)
target_compile_definitions(main PRIVATE AC_TOUCH AC_GL4ES AL_LIBTYPE_STATIC)
target_link_libraries(main PRIVATE SDL2 SDL2_image_static vorbis_static ogg OpenAL gl4es enet
                      z log android OpenSLES dl m)
