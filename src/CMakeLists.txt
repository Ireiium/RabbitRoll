option(ENABLE_AI "Build with DeepSeek AI scheduling support" OFF)

if(ENABLE_AI)
    add_definitions(-DENABLE_AI_SUPPORT)
    message(STATUS "RabbitRoll: AI Support Enabled")
else()
    message(STATUS "RabbitRoll: AI Support Disabled")
endif()

qt_add_executable(RabbitRoll
    WIN32 MACOSX_BUNDLE
    ${CMAKE_SOURCE_DIR}/Resource.qrc
    MenuButton.h
    MenuButton.cpp
    main.cpp
    mainwindow.cpp
    mainwindow.h
    componentbase.h
    componentmanager.h
    componentmanager.cpp
    memberinfocomponent.h
    memberinfocomponent.cpp
    schedulecomponent.h
    schedulecomponent.cpp
    homepage.cpp
    homepage.h
    editpage.h
    editpage.cpp
    memberpage.h
    memberpage.cpp
    memberinfomodel.h
    memberinfomodel.cpp
    memberinformationdock.h
    memberinformationdock.cpp
    memberinfodelegate.h
    memberinfodelegate.cpp
    memberinfoview.h
    memberinfoview.cpp
    windialog.h
    windialog.cpp
    dialogblank.h
    dialogblank.cpp
    dialogpropertyedit.h
    dialogpropertyedit.cpp
    dialog.h
    dialog.cpp
    dialogaction.h
    dialogaction.cpp
    RTypies.h
    schedulemodel.h
    schedulemodel.cpp
    scheduleview.h
    scheduleview.cpp
    scheduledelegate.h
    scheduledelegate.cpp
    dialogtimeselect.h
    dialogtimeselect.cpp
    memberinfocontroller.h
    memberinfocontroller.cpp
    schedulecontroller.h
    schedulecontroller.cpp
    settingpage.h
    settingpage.cpp
    aboutpage.h
    aboutpage.cpp
    editordutymodel.h
    editordutymodel.cpp
    editordutyview.h
    editordutyview.cpp
    editordutycomponent.h
    editordutycomponent.cpp
    editordutycontroller.h
    editordutycontroller.cpp
    projectpage.h
    projectpage.cpp
    idlematchservice.h
    idlematchservice.cpp
    editorselectdock.h
    editorselectdock.cpp
    projectmanager.h
    projectmanager.cpp
    projectsetting.h
    projectsetting.cpp
    dialogbatchadd.h
    dialogbatchadd.cpp
    projectworker.h
    projectworker.cpp
    backendworker.h
    backendworker.cpp
    feishuscheduleexporter.h
    feishuscheduleexporter.cpp
    feishuexportdialog.h
    feishuexportdialog.cpp
    exceltemplates.h
    exceltemplates.cpp
    exceltemplatepicker.h
    exceltemplatepicker.cpp
    RecentFileButton.h
    RecentFileButton.cpp
    projectactioncontroller.h
    projectactioncontroller.cpp
    scriptapi.h
    scriptapi.cpp
    scriptengine.h
    scriptengine.cpp
    rconsole.h
    rconsole.cpp
    undocommands.h
    undocommands.cpp
    settings.h
    settings.cpp
    lock.h
    lock.cpp
)

add_library(RabbitRollQXlsx STATIC IMPORTED GLOBAL)
set_target_properties(RabbitRollQXlsx PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${PROJECT_SOURCE_DIR}/libs/QXlsx/lib/QXlsxQt6d.lib"
    IMPORTED_LOCATION_RELEASE "${PROJECT_SOURCE_DIR}/libs/QXlsx/lib/QXlsxQt6.lib"
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/libs/QXlsx/include"
    INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Gui"
)

if(ENABLE_AI)
    target_sources(RabbitRoll PRIVATE
        AIServiceInterface.h
        AIServiceInterface.cpp
        deepseekservice.h
        deepseekservice.cpp
    )
    target_link_libraries(RabbitRoll PRIVATE Qt::Network)
endif()

if(WIN32)
    target_sources(RabbitRoll PRIVATE 
        "${CMAKE_SOURCE_DIR}/res/platform/windows/resources.rc"
    )
    set_target_properties(RabbitRoll PROPERTIES 
        RESOURCE__INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/res/platform/windows"
    )
endif()


target_link_libraries(RabbitRoll PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Concurrent
    pybind11::embed
    Python3::Python
    ElaWidgetTools
    RabbitRollQXlsx
)

# pybind11 keeps application Debug builds compatible with release CPython.
# Do not define Py_DEBUG: the bundled runtime is python312.dll.
add_custom_command(TARGET RabbitRoll POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:ElaWidgetTools>" "$<TARGET_FILE_DIR:RabbitRoll>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/pythonembed/Windows" "$<TARGET_FILE_DIR:RabbitRoll>/pythonembed"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/pythonembed/Windows/python312.dll" "$<TARGET_FILE_DIR:RabbitRoll>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:RabbitRoll>/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:RabbitRoll>/licenses/LICENSE_RabbitRoll_MIT.txt"
    COMMENT "RabbitRoll: deploying libraries, Python and licenses"
    VERBATIM
)

# This also runs when no C++ file needs rebuilding, without overwriting user scripts.
add_custom_target(RabbitRollExampleScripts
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/scripts"
        "-DDESTINATION_DIR=$<TARGET_FILE_DIR:RabbitRoll>/scripts"
        -P "${PROJECT_SOURCE_DIR}/cmake/CopyExampleScripts.cmake"
    COMMENT "RabbitRoll: checking example scripts"
    VERBATIM
)
add_dependencies(RabbitRoll RabbitRollExampleScripts)

add_custom_target(RabbitRollExcelTemplates
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/Excel template"
        "-DDESTINATION_DIR=$<TARGET_FILE_DIR:RabbitRoll>/Excel template"
        -P "${PROJECT_SOURCE_DIR}/cmake/CopyExcelTemplates.cmake"
    COMMENT "RabbitRoll: deploying Excel templates"
    VERBATIM
)
add_dependencies(RabbitRoll RabbitRollExcelTemplates)

option(RABBITROLL_DEPLOY_QT_AFTER_BUILD "Deploy Qt runtime next to the built executable" ON)
if(RABBITROLL_DEPLOY_QT_AFTER_BUILD)
    add_custom_command(TARGET RabbitRoll POST_BUILD
        COMMAND Qt6::windeployqt
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            "$<TARGET_FILE:RabbitRoll>"
        COMMENT "RabbitRoll: deploying Qt runtime"
        VERBATIM
    )
endif()
