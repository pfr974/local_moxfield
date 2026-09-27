cmake_minimum_required(VERSION 3.16)

project(local_moxfield VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6 only: an older Qt 5 elsewhere on the machine (e.g. Anaconda) must never be picked up.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

qt_add_executable(local_moxfield
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(local_moxfield PRIVATE Qt6::Widgets)

set_target_properties(local_moxfield PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.pfr974.local_moxfield
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS local_moxfield
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(local_moxfield)
