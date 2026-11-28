find_package(QtMcp 2026.7.28 EXACT CONFIG REQUIRED)
qt_add_executable(RabbitRollMcp helpermain.cpp ipcclient.cpp ipcclient.h ipcprotocol.cpp ipcprotocol.h)
target_compile_features(RabbitRollMcp PRIVATE cxx_std_20)
target_link_libraries(RabbitRollMcp PRIVATE QtMcp::Server Qt6::Network)

# windeployqt does not discover third-party backend plugins.
get_target_property(_qtmcp_dll Qt6::McpServer IMPORTED_LOCATION_RELEASE)
get_filename_component(_qtmcp_bin "${_qtmcp_dll}" DIRECTORY)
get_filename_component(_qtmcp_prefix "${_qtmcp_bin}" DIRECTORY)
set(_stdio_plugin "${_qtmcp_prefix}/plugins/mcpserverbackend/qmcpserverstdio.dll")
if(NOT EXISTS "${_stdio_plugin}")
    message(FATAL_ERROR "QtMcp stdio plugin missing; build/install the pinned QtMcp source separately with Qt BuildInternals first.")
endif()
add_custom_command(TARGET RabbitRollMcp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::McpCommon>" "$<TARGET_FILE:Qt6::McpServer>" "$<TARGET_FILE_DIR:RabbitRollMcp>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE_DIR:RabbitRollMcp>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RabbitRollMcp>/mcpserverbackend"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_stdio_plugin}" "$<TARGET_FILE_DIR:RabbitRollMcp>/mcpserverbackend"
    # Qt 6.8 windeployqt mistakes Qt6Mcp* for built-in Qt modules and searches
    # for them under the Qt SDK. Deploy Qt using its actual DLL entry points.
    COMMAND Qt6::windeployqt --release --no-translations --dir "$<TARGET_FILE_DIR:RabbitRollMcp>" "$<TARGET_FILE:Qt6::Gui>"
    COMMAND Qt6::windeployqt --release --no-translations --dir "$<TARGET_FILE_DIR:RabbitRollMcp>" "$<TARGET_FILE:Qt6::Network>"
    VERBATIM)
# Keep the agent guide next to both executables, even without a C++ rebuild.
add_custom_target(RabbitRollMcpUsage
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/MCP_USAGE.md" "$<TARGET_FILE_DIR:RabbitRoll>/MCP_USAGE.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/MCP_USAGE.md" "$<TARGET_FILE_DIR:RabbitRollMcp>/MCP_USAGE.md"
    COMMENT "RabbitRoll: updating MCP usage guide"
    VERBATIM)
add_dependencies(RabbitRoll RabbitRollMcpUsage)
add_dependencies(RabbitRollMcp RabbitRollMcpUsage)
install(FILES "${PROJECT_SOURCE_DIR}/MCP_USAGE.md" DESTINATION .)

install(TARGETS RabbitRollMcp RUNTIME DESTINATION .)
install(FILES "$<TARGET_FILE:Qt6::McpCommon>" "$<TARGET_FILE:Qt6::McpServer>" DESTINATION .)
install(FILES "${_stdio_plugin}" DESTINATION mcpserverbackend)
install(FILES "${PROJECT_SOURCE_DIR}/libs/QtMcp/qtmcp-5e97aa67496eb07c4b7ff166a22eb7684cd41742.zip" DESTINATION libs/QtMcp)
