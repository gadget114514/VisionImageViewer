cmake_minimum_required(VERSION 3.20)
project(VisionImageViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE NOMINMAX)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Use local unarr
add_subdirectory(unarr)

# Add executable
add_executable(VisionImageViewer WIN32 
    src/main.cpp
    src/PluginManager.cpp
    src/PluginManager.h
    src/ExplorerView.cpp
    src/ExplorerView.h
    src/ImageView.cpp
    src/ImageView.h
    src/ArchiveHelper.cpp
    src/ArchiveHelper.h
    src/MoveHelper.cpp
    src/MoveHelper.h
    src/resource.rc
    src/resource.h
    src/MetadataHelper.h
    src/ImageViewerWindow.h
    src/TinyEXIF/TinyEXIF.cpp
    src/TinyEXIF/TinyEXIF.h
)

target_include_directories(VisionImageViewer PRIVATE src src/TinyEXIF)
target_compile_definitions(VisionImageViewer PRIVATE TINYEXIF_NO_XMP_SUPPORT)

# Link necessary Windows libraries
target_link_libraries(VisionImageViewer PRIVATE
    user32
    gdi32
    d2d1
    dwrite
    windowscodecs
    shlwapi
    winhttp
    comctl32
    crypt32
    ole32
    nlohmann_json::nlohmann_json
    unarr
)

# Copy DLLs to output directory
add_custom_command(TARGET VisionImageViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:unarr>
    $<TARGET_FILE_DIR:VisionImageViewer>
)

# Test Application
add_executable(VisionImageViewerTests 
    src/test_main.cpp
    src/ArchiveHelper.cpp
    src/MoveHelper.cpp
    src/PluginManager.cpp
    src/resource.rc
)
target_link_libraries(VisionImageViewerTests PRIVATE
    user32 gdi32 d2d1 dwrite windowscodecs shlwapi winhttp comctl32 ole32 crypt32
    nlohmann_json::nlohmann_json
    unarr
)
