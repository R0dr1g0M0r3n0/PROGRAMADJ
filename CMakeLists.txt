cmake_minimum_required(VERSION 3.26)

project(AudioForgeStudio LANGUAGES C CXX)

include(CTest)

add_library(AudioForgeEngine STATIC
    Engine/src/engine.cpp
)
target_compile_features(AudioForgeEngine PUBLIC cxx_std_20)
target_include_directories(AudioForgeEngine PUBLIC API)

if(BUILD_TESTING)
    add_executable(AudioForgeEngineTests
        Tests/Engine/engine_api_smoke_test.c
    )
    target_link_libraries(AudioForgeEngineTests PRIVATE AudioForgeEngine)
    add_test(NAME AudioForgeEngineTests COMMAND AudioForgeEngineTests)
endif()
