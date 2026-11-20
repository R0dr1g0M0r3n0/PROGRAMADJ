option(AUDIOFORGE_ENABLE_SIGNED_XPC_TESTS "Build signed peer integration fixtures (Debug Xcode builds)" OFF)
if(NOT AUDIOFORGE_ENABLE_SIGNED_XPC_TESTS)
    return()
endif()
if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "Signed XPC integration tests require the Xcode generator")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(add_signed_peer_host target service)
    add_executable(${target} MACOSX_BUNDLE EXCLUDE_FROM_ALL live_peer_client.mm)
    target_compile_options(${target} PRIVATE -fobjc-arc)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/AgentControl/src")
    target_link_libraries(${target} PRIVATE AudioForgeAgentControl)
    add_dependencies(${target} ${service})
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.audioforge.studio
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.audioforge.studio
        XCODE_EMBED_XPC_SERVICES ${service}
    )
endfunction()

add_executable(AudioForgeWrongService MACOSX_BUNDLE EXCLUDE_FROM_ALL live_peer_service.mm)
target_compile_options(AudioForgeWrongService PRIVATE -fobjc-arc)
target_include_directories(AudioForgeWrongService PRIVATE "${PROJECT_SOURCE_DIR}/AgentControl/src")
target_link_libraries(AudioForgeWrongService PRIVATE AudioForgeAgentControl)
set_target_properties(AudioForgeWrongService PROPERTIES
    OUTPUT_NAME "${agent_control_service_id}"
    BUNDLE_EXTENSION xpc
    MACOSX_BUNDLE_GUI_IDENTIFIER "${agent_control_service_id}"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/AgentControl/ServiceInfo.plist.in"
    XCODE_PRODUCT_TYPE com.apple.product-type.xpc-service
    XCODE_EXPLICIT_FILE_TYPE wrapper.xpc-service
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION xpc
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${agent_control_service_id}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
)

add_signed_peer_host(AudioForgeWrongServiceHost AudioForgeWrongService)
add_signed_peer_host(AudioForgeWrongClientHost AudioForgeAgentControlService)
add_signed_peer_host(AudioForgeMatchingClientHost AudioForgeAgentControlService)

# Change only the signing identifiers so launchd sees the usual bundle and service names.
set_property(TARGET AudioForgeWrongService PROPERTY
    XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--identifier ${agent_control_service_id}.f1-test")
set_property(TARGET AudioForgeWrongClientHost PROPERTY
    XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--identifier com.audioforge.studio.f1-test")

add_custom_target(AudioForgeAgentControlLivePeerTests)
add_dependencies(AudioForgeAgentControlLivePeerTests
    AudioForge AudioForgeWrongServiceHost AudioForgeWrongClientHost AudioForgeMatchingClientHost)
add_test(NAME AudioForgeAgentControlLivePeerTests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/live_peer_test.py"
        "$<TARGET_BUNDLE_DIR:AudioForge>"
        "$<TARGET_BUNDLE_DIR:AudioForgeWrongServiceHost>"
        "$<TARGET_BUNDLE_DIR:AudioForgeWrongClientHost>"
        "$<TARGET_BUNDLE_DIR:AudioForgeMatchingClientHost>"
    CONFIGURATIONS Debug)
set_tests_properties(AudioForgeAgentControlLivePeerTests PROPERTIES TIMEOUT 90)
