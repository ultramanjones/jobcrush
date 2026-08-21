cmake_minimum_required(VERSION 3.21)

project(jobcrush VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(jobcrush
    src/main.cpp
)

qt_add_qml_module(jobcrush
    URI JobCrush
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(jobcrush PRIVATE Qt6::Quick)
