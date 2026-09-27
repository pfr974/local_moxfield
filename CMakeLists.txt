cmake_minimum_required(VERSION 3.16)

project(local_moxfield VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6 only: an older Qt 5 elsewhere on the machine (e.g. Anaconda) must never be picked up.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Sql Concurrent Test)
find_package(ZLIB REQUIRED)

# Card data, storage and networking: no UI, so tests can link it directly.
add_library(mtg_core STATIC
    src/core/bulkdatadownloader.cpp
    src/core/bulkdatadownloader.h
    src/core/card.cpp
    src/core/card.h
    src/core/carddatabase.cpp
    src/core/carddatabase.h
    src/core/gziplinereader.cpp
    src/core/gziplinereader.h
)
target_include_directories(mtg_core PUBLIC src)
target_compile_definitions(mtg_core PUBLIC LOCAL_MOXFIELD_VERSION="${PROJECT_VERSION}")
target_link_libraries(mtg_core PUBLIC Qt6::Core Qt6::Network Qt6::Sql ZLIB::ZLIB)

qt_add_executable(local_moxfield
    MANUAL_FINALIZATION
    src/main.cpp
    src/ui/carddetails.cpp
    src/ui/carddetails.h
    src/ui/cardlistmodel.cpp
    src/ui/cardlistmodel.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
)

target_link_libraries(local_moxfield PRIVATE mtg_core Qt6::Widgets Qt6::Concurrent)

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

# Tests: run with `ctest` from the build directory, or from Qt Creator's Tests pane.
enable_testing()
foreach(test_name tst_card tst_carddatabase)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE mtg_core Qt6::Test ZLIB::ZLIB)
    target_compile_definitions(${test_name} PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
