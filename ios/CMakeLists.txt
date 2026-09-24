cmake_minimum_required(VERSION 3.20)

project(th095_ios LANGUAGES C CXX OBJC OBJCXX)

# CMake 4.x removed the compatibility policy floor used by the vendored
# FreeType copy pulled in by SDL_ttf.  Keep the iOS build reproducible on the
# Mac's current CMake while leaving the app itself on the modern policy set.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING
        "Compatibility floor for vendored SDL/FreeType subprojects" FORCE)
endif()

set(TH095_IOS_VERSION "0.1.0" CACHE STRING "iOS marketing version")
set(TH095_IOS_BUILD "1" CACHE STRING "iOS build number")
set(TH095_IOS_BUNDLE_ID "com.th095.reconstruction" CACHE STRING "Bundle identifier")
option(TH095_FULL_PORT "Build the reconstructed TH095 runtime instead of the bring-up shell" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Use an iOS CMake toolchain: -DCMAKE_SYSTEM_NAME=iOS")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum iOS version")

set(APP_SOURCES
    main.mm
    TH095AppDelegate.mm
    TH095View.mm
    TH095ViewController.mm
    TH095Runtime.cpp
    TH095AssetReader.cpp
)

if(TH095_FULL_PORT)
    file(GLOB TH095_GAME_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/ecl/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/pbg/*.cpp")
    list(FILTER TH095_GAME_SOURCES EXCLUDE REGEX "/(Main|MainExact)\.cpp$")
    list(APPEND TH095_GAME_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_runtime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_d3d8_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_d3dx8_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_gl_legacy.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_touch_portable.cpp")
    set(APP_SOURCES ${TH095_GAME_SOURCES})
endif()

add_executable(th095_ios MACOSX_BUNDLE ${APP_SOURCES})
file(GLOB TH095_APP_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon*.png")
target_sources(th095_ios PRIVATE ${TH095_APP_ICONS})
set_source_files_properties(${TH095_APP_ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(th095_ios PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "${TH095_IOS_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "TH095"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${TH095_IOS_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${TH095_IOS_BUILD}"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${TH095_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
)

# CMake 4.4 on the pinned Xcode 14 toolchain does not expose the usual
# AppleClang feature table when cross-configuring an iOS Xcode project.  Use
# the language standard properties directly; this still emits -std=c++17 but
# avoids target_compile_features rejecting an otherwise valid compiler.
set_target_properties(th095_ios PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
target_compile_definitions(th095_ios PRIVATE TH095_IOS_PORT=1)
if(TH095_FULL_PORT)
    target_compile_definitions(th095_ios PRIVATE TH095_MODERN_PORT TH095_MODERN_IOS TH095_IOS TH095_IOS_PORTABLE_LAYOUT TH08_MODERN_PORT)
    target_compile_options(th095_ios PRIVATE
        "$<$<CONFIG:Debug>:-O0>" "$<$<NOT:$<CONFIG:Debug>>:-O2>"
        -fno-fast-math -fno-strict-aliasing
        "-include${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios/ios_compat.hpp")
    target_include_directories(th095_ios BEFORE PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/linux/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/linux"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/modern/ios"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src")
else()
    target_include_directories(th095_ios PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
target_link_libraries(th095_ios PRIVATE
    "-framework UIKit"
    "-framework Foundation"
    "-framework CoreGraphics"
    "-framework CoreText"
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
)

if(TH095_FULL_PORT)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER ON CACHE BOOL "" FORCE)
    set(SDL_METAL OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER_METAL OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
    # The portable source bundle does not include SDL_mixer's optional
    # WavPack checkout. TH095 uses WAV/OGG assets on iOS, so keep this
    # decoder disabled and avoid a network-only configure failure.
    set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_VORBIS STB CACHE STRING "" FORCE)
    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.30.9)
    FetchContent_Declare(SDL2_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git GIT_TAG release-2.8.2)
    FetchContent_Declare(SDL2_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG release-2.8.0)
    FetchContent_Declare(SDL2_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG release-2.22.0)
    FetchContent_MakeAvailable(SDL2 SDL2_image SDL2_mixer SDL2_ttf)
    if(TARGET SDL2-static AND NOT TARGET SDL2::SDL2-static)
        add_library(SDL2::SDL2-static ALIAS SDL2-static)
    endif()
    if(TARGET SDL2_image AND NOT TARGET SDL2_image::SDL2_image)
        add_library(SDL2_image::SDL2_image ALIAS SDL2_image)
    endif()
    if(TARGET SDL2_mixer AND NOT TARGET SDL2_mixer::SDL2_mixer)
        add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_mixer)
    endif()
    if(TARGET SDL2_ttf AND NOT TARGET SDL2_ttf::SDL2_ttf)
        add_library(SDL2_ttf::SDL2_ttf ALIAS SDL2_ttf)
    endif()
    target_link_libraries(th095_ios PRIVATE SDL2::SDL2main SDL2::SDL2-static
        SDL2_image::SDL2_image SDL2_mixer::SDL2_mixer SDL2_ttf::SDL2_ttf
        "-framework OpenGLES" "-framework CoreFoundation" "-framework AudioToolbox"
        "-framework AVFoundation" "-framework CoreAudio")
endif()

# Original game data is not vendored in the repository. The build copies the
# Japanese th095.dat/thbgm.dat into the app bundle only when the operator has
# staged them under ios/resources. Both are shipped inside the IPA and are
# never committed to source control.
set(TH095_BUNDLED_DATA_FILES "")
foreach(TH095_DATA_NAME th095.dat thbgm.dat)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/${TH095_DATA_NAME}")
        list(APPEND TH095_BUNDLED_DATA_FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/${TH095_DATA_NAME}")
    endif()
endforeach()

add_custom_command(TARGET th095_ios POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        "$<TARGET_BUNDLE_DIR:th095_ios>/Info.plist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon32.png"
        "$<TARGET_BUNDLE_DIR:th095_ios>/AppIcon32.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon60.png"
        "$<TARGET_BUNDLE_DIR:th095_ios>/AppIcon60.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon76.png"
        "$<TARGET_BUNDLE_DIR:th095_ios>/AppIcon76.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon83.5.png"
        "$<TARGET_BUNDLE_DIR:th095_ios>/AppIcon83.5.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/AppIcon1024.png"
        "$<TARGET_BUNDLE_DIR:th095_ios>/AppIcon1024.png"
)

if(TH095_BUNDLED_DATA_FILES)
    add_custom_command(TARGET th095_ios POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TH095_BUNDLED_DATA_FILES}
            "$<TARGET_BUNDLE_DIR:th095_ios>/"
        COMMENT "Bundling TH095 game data")
endif()
