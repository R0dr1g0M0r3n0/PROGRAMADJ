cmake_minimum_required(VERSION 3.29)

# Keep the app and both native libraries on the same deployment baseline.
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
project(AudioForgeStudio LANGUAGES C CXX OBJCXX Swift)

include(CTest)
find_package(nlohmann_json CONFIG REQUIRED)

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

add_library(AudioForgeProjectPersistence STATIC
    ProjectPersistence/src/project_store.cpp
)
target_compile_features(AudioForgeProjectPersistence PUBLIC cxx_std_20)
target_include_directories(AudioForgeProjectPersistence PUBLIC ProjectPersistence/include)
target_link_libraries(AudioForgeProjectPersistence PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(AudioForgeProjectPersistence PRIVATE
    PROJECT_MANIFEST_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/docs/project-manifest-v1.schema.json"
)

add_library(AudioForgeBridge STATIC
    Bridge/src/AudioForgeBridge.mm
)
target_include_directories(AudioForgeBridge PUBLIC Bridge/include)
target_include_directories(AudioForgeBridge PRIVATE API)
target_link_libraries(AudioForgeBridge PUBLIC AudioForgeEngine "-framework Foundation")

set(agent_control_service_id com.audioforge.studio.agent-control)
add_library(AudioForgeAgentControl STATIC
    AgentControl/src/AgentControlClient.mm
    AgentControl/src/AgentControlIdentity.mm
    AgentControl/src/AgentControlWire.mm
)
target_compile_options(AudioForgeAgentControl PRIVATE -fobjc-arc)
target_include_directories(AudioForgeAgentControl PUBLIC AgentControl/include)
target_link_libraries(AudioForgeAgentControl PUBLIC "-framework Foundation" "-framework Security")

add_executable(AudioForgeAgentControlService MACOSX_BUNDLE AgentControl/src/AgentControlService.mm)
target_compile_options(AudioForgeAgentControlService PRIVATE -fobjc-arc)
target_link_libraries(AudioForgeAgentControlService PRIVATE AudioForgeAgentControl)
set_target_properties(AudioForgeAgentControlService PROPERTIES
    OUTPUT_NAME "${agent_control_service_id}"
    BUNDLE_EXTENSION xpc
    MACOSX_BUNDLE_GUI_IDENTIFIER "${agent_control_service_id}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/AgentControl/ServiceInfo.plist.in"
    XCODE_PRODUCT_TYPE com.apple.product-type.xpc-service
    XCODE_EXPLICIT_FILE_TYPE wrapper.xpc-service
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION xpc
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${agent_control_service_id}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
)

add_executable(AudioForge MACOSX_BUNDLE
    App/AudioForgeApp.swift
)
target_link_libraries(AudioForge PRIVATE AudioForgeBridge AudioForgeAgentControl)
target_compile_definitions(AudioForge PRIVATE "$<$<CONFIG:Debug>:AF_XPC_DISCOVERY_PROBE>")
add_dependencies(AudioForge AudioForgeAgentControlService)
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_property(TARGET AudioForge PROPERTY XCODE_EMBED_XPC_SERVICES AudioForgeAgentControlService)
else()
    set_property(TARGET AudioForge APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:AudioForgeAgentControlService>")
    set(embedded_service "$<TARGET_BUNDLE_DIR:AudioForge>/Contents/XPCServices/${agent_control_service_id}.xpc")
    add_custom_command(TARGET AudioForge POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${embedded_service}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:AudioForgeAgentControlService>" "${embedded_service}"
        VERBATIM
    )
endif()
set_target_properties(AudioForge PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.audioforge.studio
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.audioforge.studio
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_SOURCE_DIR}/App/AudioForge-Bridging-Header.h"
    XCODE_ATTRIBUTE_SWIFT_VERSION 5.0
)

if(BUILD_TESTING)
    add_executable(AudioForgeAgentControlTests Tests/AgentControl/agent_control_test.mm)
    target_compile_options(AudioForgeAgentControlTests PRIVATE -fobjc-arc)
    target_include_directories(AudioForgeAgentControlTests PRIVATE AgentControl/src)
    target_link_libraries(AudioForgeAgentControlTests PRIVATE AudioForgeAgentControl)
    add_dependencies(AudioForgeAgentControlTests AudioForge)
    add_test(NAME AudioForgeAgentControlTests
        COMMAND AudioForgeAgentControlTests "$<TARGET_BUNDLE_DIR:AudioForge>")
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

    add_executable(AudioForgeProjectPersistenceTests
        Tests/ProjectPersistence/project_store_test.cpp
    )
    target_link_libraries(AudioForgeProjectPersistenceTests PRIVATE AudioForgeProjectPersistence)
    target_compile_features(AudioForgeProjectPersistenceTests PRIVATE cxx_std_20)
    target_compile_definitions(AudioForgeProjectPersistenceTests PRIVATE
        PROJECT_MANIFEST_FIXTURE="${CMAKE_SOURCE_DIR}/TestFixtures/project_manifest_v1.json"
    )
    add_test(NAME AudioForgeProjectPersistenceTests COMMAND AudioForgeProjectPersistenceTests)

    set_tests_properties(AudioForgeEngineTests AudioForgeBridgeTests
        PROPERTIES LABELS "api-contract"
    )
endif()
