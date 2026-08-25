cmake_minimum_required(VERSION 3.16)

project(SimpleEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to an optimized Release build when the user doesn't specify one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network)

# Find QScintilla
find_library(QSCINTILLA_LIB NAMES qscintilla2_qt6)
if(NOT QSCINTILLA_LIB)
    message(FATAL_ERROR "QScintilla2 for Qt6 (qscintilla2_qt6) not found. Please install it.")
endif()

add_executable(SimpleEditor
    src/main.cpp
    src/mainwindow.cpp
    src/codeeditor.cpp
    src/sessionmanager.cpp
    src/searchdialog.cpp
    src/themedialog.cpp
    src/mainwindow.h
    src/codeeditor.h
    src/sessionmanager.h
    src/searchdialog.h
    src/themedialog.h
)

target_link_libraries(SimpleEditor PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network ${QSCINTILLA_LIB})

# Set output directory for scripts to find
set_target_properties(SimpleEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
