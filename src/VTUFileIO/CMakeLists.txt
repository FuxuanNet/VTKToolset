set(CMAKE_AUTOMOC ON) 
set(CMAKE_INCLUDE_CURRENT_DIR ON) 

# directories
include_directories(
    ${LIBS_QT5_ROOT}/include
    ${LIBS_QT5_ROOT}/include/QtCore
    ${LIBS_PYTHON_ROOT}/include
    ${LIBS_OC_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include
    ${LIBS_SAMSDK_ROOT}/include/guiProject
    ${LIBS_HDF5_ROOT}/include
)
  
link_directories(
    ${LIBS_QT5_ROOT}/lib
    ${LIBS_PYTHON_ROOT}/libs
    ${LIBS_OC_ROOT}/lib
    ${LIBS_SAMSDK_ROOT}/libs
    ${LIBS_HDF5_ROOT}/lib
	${PROJECT_SOURCE_DIR}/lib
    ${LIBS_TCMALLOC_ROOT}
    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)

# source list
file(GLOB TARGET_SRC
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.T"
    )
     
# Moc Qt headers
set(HEADERS
)

QT5_WRAP_CPP(HEADERS_MOC ${HEADERS})
QT5_ADD_RESOURCES(RESOUCES_RCC ${RESOUCES})


# add filters
source_group("Generated Files" FILES
    ${HEADERS_MOC}
)

# dynamic library
add_library(VTUFileIO SHARED
    ${TARGET_SRC}
    ${HEADERS_MOC}
    ${RESOUCES_RCC}
)

# compiler settings
set_property(TARGET VTUFileIO
    PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS _CRT_NON_CONFORMING_SWPRINTFS H5_BUILT_AS_DYNAMIC_LIB
)

set_target_properties(VTUFileIO PROPERTIES LINK_FLAGS /INCLUDE:"__tcmalloc")

add_definitions(
    -DUNICODE
    -D_UNICODE
)

# linker settings
set_target_properties(VTUFileIO PROPERTIES
    SUFFIX ".pyd")

target_link_libraries(VTUFileIO
    Qt5Core.lib
    OrientBasicContainers
    OrientBasicUtilities
    OrientBasicCoreUtilities
    OrientBasicPrimtives
    OrientPythonInterfaces
    SAMSceneGraph
    SAMModelDatabase
    SAMPythonModuleConfiguration
    SAMPrimaryObjects
    OrientElementShapes
    SAMCoreMeshDefs
    SAMKernel
    SAMGeometryComponent
    SAMGeometryCoreModule
    SAMKernelAttributesInterfaceObjects
    SAMCommonAttributes
    SAMPythonUtilities
    SAMKernelComponents
    SAMCommonCommand
    SAMCoreAttributes
    SAMMeshInterfaceObjects
    SAMOdbCoreResults
    SAMOdbInterface
    SAMOdbPythonInterface
    hdf5.lib
    libtcmalloc_minimal.lib
)

add_custom_command(TARGET VTUFileIO POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${LIBS_SAM_ROOT}/Release
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${PROJECT_SOURCE_DIR}/deploy/Release
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:VTUFileIO>
    ${LIBS_SAM_ROOT}/Release/VTUFileIO.pyd
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:VTUFileIO>
    ${PROJECT_SOURCE_DIR}/deploy/Release/VTUFileIO.pyd
    VERBATIM
)

add_executable(VTUFileIOSmokeTest
    ${PROJECT_SOURCE_DIR}/tests/direct_h5_export_smoke.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/SamH5VtkExporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VTUDataContainer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VTUElementHandler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VTKLegacyFormatIO.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/FormatIO.cpp
)

target_include_directories(VTUFileIOSmokeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBS_QT5_ROOT}/include
    ${LIBS_QT5_ROOT}/include/QtCore
    ${LIBS_SAMSDK_ROOT}/include
	${LIBS_HDF5_ROOT}/include
)

target_link_libraries(VTUFileIOSmokeTest
    Qt5Core.lib
	hdf5.lib
)

target_compile_definitions(VTUFileIOSmokeTest PRIVATE H5_BUILT_AS_DYNAMIC_LIB)
