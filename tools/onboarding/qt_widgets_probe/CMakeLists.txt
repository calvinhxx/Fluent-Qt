cmake_minimum_required(VERSION 3.16)

project(FluentQtDoctorQtWidgetsProbe LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(_qt_version_variable "Qt${QT_VERSION_MAJOR}_VERSION")
set(_qt_dir_variable "Qt${QT_VERSION_MAJOR}_DIR")

message(STATUS "FLUENTQT_DOCTOR_QT_MAJOR=${QT_VERSION_MAJOR}")
message(STATUS "FLUENTQT_DOCTOR_QT_VERSION=${${_qt_version_variable}}")
message(STATUS "FLUENTQT_DOCTOR_QT_DIR=${${_qt_dir_variable}}")
message(STATUS "FLUENTQT_DOCTOR_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
