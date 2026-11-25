cmake_minimum_required(VERSION 3.25)
project(OpenAudioEngine LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(oae_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

set(oae_runtime_sources
    native/src/api/bank_api.cpp
    native/src/api/bus_api.cpp
    native/src/api/effect_api.cpp
    native/src/api/event_api.cpp
    native/src/api/runtime.cpp
    native/src/api/sound_api.cpp
    native/src/api/system_api.cpp
    native/src/api/vca_api.cpp
    native/src/core/bank.cpp
    native/src/core/bus.cpp
    native/src/core/bus_dsp.cpp
    native/src/core/effect.cpp
    native/src/core/event_instance.cpp
    native/src/core/channel.cpp
    native/src/core/sound.cpp
    native/src/core/strip.cpp
    native/src/core/studio_model.cpp
    native/src/core/system.cpp
    native/src/core/vca.cpp
    native/src/studio/bank_reader.cpp
    native/src/studio/bank_writer.cpp)
add_library(oae_audio_static STATIC native/src/miniaudio_impl.c ${oae_runtime_sources})
target_compile_definitions(oae_audio_static PUBLIC OAE_AUDIO_STATIC)
set_target_properties(oae_audio_static PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

add_library(oae_audio SHARED native/src/miniaudio_impl.c ${oae_runtime_sources})
target_compile_definitions(oae_audio PRIVATE OAE_AUDIO_BUILD)
set_target_properties(oae_audio PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(oae_audio PUBLIC "${PROJECT_SOURCE_DIR}/native/include")
target_include_directories(oae_audio SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
oae_enable_warnings(oae_audio)
target_include_directories(oae_audio_static PUBLIC "${PROJECT_SOURCE_DIR}/native/include")
target_include_directories(oae_audio_static SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
oae_enable_warnings(oae_audio_static)

set(oae_project_schema "${PROJECT_SOURCE_DIR}/docs/specs/project-v1.schema.proposed.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${oae_project_schema}")
file(READ "${oae_project_schema}" OAE_PROJECT_SCHEMA_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," OAE_PROJECT_SCHEMA_BYTES "${OAE_PROJECT_SCHEMA_HEX}")
configure_file(native/tools/bankc/project_schema.h.in generated/project_schema.h @ONLY)
add_executable(oae_bankc native/tools/bankc/main.cpp)
add_library(oae_publication STATIC native/tools/bankc/publication.cpp)
target_include_directories(oae_publication SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
target_link_libraries(oae_publication PRIVATE oae_audio_static)
oae_enable_warnings(oae_publication)
target_include_directories(oae_bankc PUBLIC "${PROJECT_SOURCE_DIR}/native/include")
target_include_directories(oae_bankc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(oae_bankc SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
target_link_libraries(oae_bankc PRIVATE oae_audio_static oae_publication)
oae_enable_warnings(oae_bankc)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(oae_audio_static PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
    target_link_libraries(oae_audio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

include(CTest)
if(BUILD_TESTING)
    file(GLOB oae_test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/native/tests/*.cpp")
    add_executable(oae_publication_probe native/tests/helpers/publication_probe.cpp)
    target_include_directories(oae_publication_probe SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
    target_link_libraries(oae_publication_probe PRIVATE oae_audio_static oae_publication)
    oae_enable_warnings(oae_publication_probe)

    add_executable(oae_native_tests ${oae_test_sources})
    target_include_directories(oae_native_tests SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/native/external")
    target_link_libraries(oae_native_tests PRIVATE oae_audio_static oae_publication)
    target_compile_definitions(oae_native_tests PRIVATE OAE_TEST_FIXTURES="${PROJECT_SOURCE_DIR}/native/tests/fixtures")
    oae_enable_warnings(oae_native_tests)
    add_dependencies(oae_native_tests oae_publication_probe)
    add_test(NAME native COMMAND oae_native_tests)
    set_tests_properties(native PROPERTIES TIMEOUT 120)

    add_executable(oae_c_api_smoke native/tests/c_api_smoke.c)
    target_link_libraries(oae_c_api_smoke PRIVATE oae_audio)
    oae_enable_warnings(oae_c_api_smoke)
    add_test(NAME c_api_smoke COMMAND oae_c_api_smoke)
endif()

option(BUILD_STUDIO "Build OpenAudio Studio DAW" ON)
if(BUILD_STUDIO AND EXISTS "${PROJECT_SOURCE_DIR}/studio/CMakeLists.txt")
    add_subdirectory(studio)
endif()
