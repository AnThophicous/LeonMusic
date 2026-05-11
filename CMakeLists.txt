cmake_minimum_required(VERSION 3.22)

project(LeonMusic
    VERSION 0.1.0
    DESCRIPTION "Cross-platform shared library and CLI for LeonMusic"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(LEONMUSIC_BUILD_SHARED "Build LeonMusic shared library" ON)
option(LEONMUSIC_BUILD_CLI "Build lma CLI" ON)
option(LEONMUSIC_ENABLE_DISCORD_RPC "Enable Discord Rich Presence integration" ON)
option(LEONMUSIC_ENABLE_CURL "Enable libcurl integration" OFF)
option(LEONMUSIC_ENABLE_TAGLIB "Enable TagLib metadata extraction" ON)
option(LEONMUSIC_ENABLE_PACKAGING "Enable install and packaging targets" ON)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(nlohmann_json)
FetchContent_MakeAvailable(miniaudio)

if(LEONMUSIC_ENABLE_TAGLIB)
    find_package(TagLib CONFIG QUIET)
    if(NOT TagLib_FOUND AND NOT WIN32)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            taglib
            URL https://taglib.org/releases/taglib-2.2.1.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(taglib)
    endif()
endif()

if(LEONMUSIC_ENABLE_CURL)
    find_package(CURL QUIET)
endif()

set(LEONMUSIC_PUBLIC_HEADERS
    include/leonmusic/api.hpp
    include/leonmusic/c_api.h
    include/leonmusic/version.hpp
)

set(LEONMUSIC_SOURCES
    src/api.cpp
    src/audio_player.cpp
    src/cli_command.cpp
    src/discord_rpc.cpp
    src/http_client.cpp
    src/ipc_server.cpp
    src/json_rpc_server.cpp
    src/library_service.cpp
    src/media_pipeline.cpp
    src/platform_utils.cpp
    src/process_utils.cpp
    src/search_provider.cpp
)

if(WIN32)
    list(APPEND LEONMUSIC_SOURCES src/platform/windows_ipc.cpp)
else()
    list(APPEND LEONMUSIC_SOURCES src/platform/unix_ipc.cpp)
endif()

if(LEONMUSIC_BUILD_SHARED)
    add_library(leonmusic SHARED ${LEONMUSIC_SOURCES} ${LEONMUSIC_PUBLIC_HEADERS})
    if(WIN32)
        set_target_properties(leonmusic PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
else()
    add_library(leonmusic STATIC ${LEONMUSIC_SOURCES} ${LEONMUSIC_PUBLIC_HEADERS})
endif()
add_library(LeonMusic::leonmusic ALIAS leonmusic)

target_include_directories(leonmusic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(leonmusic
    PUBLIC
        nlohmann_json::nlohmann_json)

target_include_directories(leonmusic PRIVATE ${miniaudio_SOURCE_DIR})

if(UNIX AND NOT APPLE)
    target_link_libraries(leonmusic PRIVATE m pthread dl)
endif()

if(LEONMUSIC_ENABLE_CURL AND CURL_FOUND)
    target_link_libraries(leonmusic PUBLIC CURL::libcurl)
    target_compile_definitions(leonmusic PRIVATE LEONMUSIC_HAS_CURL=1)
endif()

if(LEONMUSIC_ENABLE_TAGLIB)
    if(TARGET TagLib::tag)
        target_link_libraries(leonmusic PRIVATE TagLib::tag)
        target_compile_definitions(leonmusic PRIVATE LEONMUSIC_HAS_TAGLIB=1)
    elseif(TARGET tag)
        target_link_libraries(leonmusic PRIVATE tag)
        target_compile_definitions(leonmusic PRIVATE LEONMUSIC_HAS_TAGLIB=1)
    endif()
endif()

target_compile_definitions(leonmusic
    PUBLIC LEONMUSIC_ENABLE_DISCORD_RPC=$<BOOL:${LEONMUSIC_ENABLE_DISCORD_RPC}>)

if(WIN32)
    target_compile_definitions(leonmusic PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(LEONMUSIC_BUILD_CLI)
    add_executable(lma cli/main.cpp)
    target_link_libraries(lma PRIVATE LeonMusic::leonmusic)
endif()

if(LEONMUSIC_ENABLE_PACKAGING)
    include(GNUInstallDirs)

    install(TARGETS leonmusic
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    if(TARGET lma)
        install(TARGETS lma
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/LeonMusic)

    set(CPACK_PACKAGE_NAME "LeonMusic")
    set(CPACK_PACKAGE_VENDOR "OpenAI Codex Experiment")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform shared library and CLI for LeonMusic")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_CONTACT "OpenAI Codex")
    set(CPACK_PACKAGE_FILE_NAME "LeonMusic-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    if(WIN32)
        set(CPACK_GENERATOR "ZIP")
    elseif(APPLE)
        set(CPACK_GENERATOR "TGZ")
    else()
        set(CPACK_GENERATOR "TGZ;DEB")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenAI Codex")
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "ffmpeg, yt-dlp")
    endif()

    include(CPack)
endif()
