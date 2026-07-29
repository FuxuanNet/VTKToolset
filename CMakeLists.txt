cmake_minimum_required(VERSION 3.28.0)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(VTKToolset)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBS_SAM_ROOT "E:/SAM" CACHE PATH "SAM install root")
set(LIBS_SAMSDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../SAMSDK" CACHE PATH "SAM SDK root")

if(NOT EXISTS "${LIBS_SAM_ROOT}/Release")
    message(FATAL_ERROR "SAM installation is invalid: ${LIBS_SAM_ROOT}. Set LIBS_SAM_ROOT to the SAM install directory.")
endif()

if(NOT EXISTS "${LIBS_SAMSDK_ROOT}/include" OR
   NOT EXISTS "${LIBS_SAMSDK_ROOT}/libs" OR
   NOT EXISTS "${LIBS_SAMSDK_ROOT}/ThridPartys")
    message(FATAL_ERROR "SAM SDK is invalid: ${LIBS_SAMSDK_ROOT}. Set LIBS_SAMSDK_ROOT to the directory containing include, libs and ThridPartys.")
endif()
set(LIBS_PYTHON_ROOT ${LIBS_SAM_ROOT}/Python27)
set(LIBS_QT5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qt-5.12.6-VC14-64)
set(LIBS_OCCT_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OCCT)
set(LIBS_TCMALLOC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/TCMalloc)
set(LIBS_LEVELDB_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/libdb)
set(LIBS_HDF5_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/HDF5-1.12)
set(LIBS_CM2MeshTool_Root ${LIBS_SAMSDK_ROOT}/ThridPartys/cm2)
set(LIBS_OC_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OrientCommon)
set(LIBS_QTITAN_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/Qtitan)

add_compile_definitions(NEW_GUI_FRAME)
add_compile_definitions(NEW_RENDER_FRAME)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Widgets)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Core)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBS_QT5_ROOT}/lib/cmake/Qt5Gui)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

add_subdirectory(src/VTUFileIO)
add_subdirectory(src/VTUFileIOToolset)
