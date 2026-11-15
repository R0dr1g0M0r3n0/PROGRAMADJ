cmake_minimum_required(VERSION 3.26)

# Keep the app and both native libraries on the same deployment baseline.
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
project(AudioForgeStudio LANGUAGES C CXX OBJCXX Swift)

include(CTest)

add_library(AudioForgeEngine STATIC
    Engine/src/engine.cpp
)
target_compile_features(AudioForgeEngine PUBLIC cxx_std_20)
target_include_directories(AudioForgeEngine PUBLIC API)

add_library(AudioForgeDJLibrary STATIC
    DJLibrary/src/library.cpp
)
target_compile_features(AudioForgeDJLibrary PUBLIC cxx_std_20)
target_include_directories(AudioForgeDJLibrary PUBLIC DJLibrary/include)

add_library(AudioForgeBridge STATIC
    Bridge/src/AudioForgeBridge.mm
)
target_include_directories(AudioForgeBridge PUBLIC Bridge/include)
target_include_directories(AudioForgeBridge PRIVATE API)
target_link_libraries(AudioForgeBridge PUBLIC AudioForgeEngine "-framework Foundation")

add_executable(AudioForge MACOSX_BUNDLE
    App/AudioForgeApp.swift
)
target_link_libraries(AudioForge PRIVATE AudioForgeBridge)
set_target_properties(AudioForge PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.audioforge.studio
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.audioforge.studio
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_SOURCE_DIR}/App/AudioForge-Bridging-Header.h"
    XCODE_ATTRIBUTE_SWIFT_VERSION 5.0
)

if(BUILD_TESTING)
    add_executable(AudioForgeDJLibraryTests
        Tests/DJLibrary/library_boundary_smoke_test.cpp
    )
    target_link_libraries(AudioForgeDJLibraryTests PRIVATE AudioForgeDJLibrary)
    add_test(NAME AudioForgeDJLibraryTests COMMAND AudioForgeDJLibraryTests)

    add_executable(AudioForgeEngineTests
        Tests/Engine/engine_api_smoke_test.c
    )
    target_link_libraries(AudioForgeEngineTests PRIVATE AudioForgeEngine)
    add_test(NAME AudioForgeEngineTests COMMAND AudioForgeEngineTests)

    add_executable(AudioForgeBridgeTests
        Tests/Bridge/bridge_smoke_test.mm
    )
    target_link_libraries(AudioForgeBridgeTests PRIVATE AudioForgeBridge)
    add_test(NAME AudioForgeBridgeTests COMMAND AudioForgeBridgeTests)
endif()
