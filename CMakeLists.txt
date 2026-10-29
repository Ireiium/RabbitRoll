cmake_minimum_required(VERSION 3.19)
project(RabbitRoll LANGUAGES CXX)

# The bundled ElaWidgetTools, QXlsx and Python binaries use this toolchain.
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Bundled dependencies require Windows x64 and MSVC. Rebuild the dependencies before using another platform or compiler.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MSVC_CXX_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "Select the MSVC x64 kit; the bundled libraries do not support x86 or ARM64.")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Widgets Concurrent Network)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

find_package(Python3 3.12 EXACT COMPONENTS Interpreter Development REQUIRED)
# All app configurations use the bundled release CPython 3.12 runtime.
set_property(TARGET Python3::Python PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}" CACHE INTERNAL "")

add_definitions(-DQT_NO_KEYWORDS)
add_subdirectory(libs/pybind11)
find_package(ElaWidgetTools CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/libs/ElaWidgetTools/lib/cmake"
    NO_DEFAULT_PATH
)
add_subdirectory(src)

include(GNUInstallDirs)
install(TARGETS RabbitRoll
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(FILES "$<TARGET_FILE:ElaWidgetTools>"
    "${CMAKE_CURRENT_SOURCE_DIR}/pythonembed/Windows/python312.dll"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pythonembed/Windows/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/pythonembed"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/scripts"
    FILES_MATCHING PATTERN "*.py"
    PATTERN "__pycache__" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/licenses"
)

# Quote the executable path so Qt deployment also works in directories with spaces.
qt_generate_deploy_script(
    TARGET RabbitRoll
    OUTPUT_SCRIPT deploy_script
    CONTENT [[
qt_deploy_runtime_dependencies(
    EXECUTABLE "$<TARGET_FILE:RabbitRoll>"
    GENERATE_QT_CONF
)
]]
)
install(SCRIPT "${deploy_script}")
