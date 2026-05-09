cmake_minimum_required(VERSION 3.10)
project(QtOpenCVCamera)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME} main.cpp cameraworker.cpp cameraworker.h mainwindow.cpp mainwindow.h videolabel.cpp videolabel.h yoloassist.cpp yoloassist.h resources/icon.rc)
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets ${OpenCV_LIBS})
