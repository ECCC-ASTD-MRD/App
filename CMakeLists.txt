cmake_minimum_required(VERSION 3.16)

#----- Append EC specific module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_rpn/modules $ENV{EC_CMAKE_MODULE_PATH})

include(ec_init)           # Initialise compilers and ec specific functions
ec_git_version()           # Get version from git state
ec_parse_manifest()        # Parse MANIFEST file

project(${NAME} DESCRIPTION "${DESCRIPTION}" LANGUAGES C Fortran)
set(PROJECT_VERSION ${VERSION}${STATE})
set(${NAME}_VERSION ${PROJECT_VERSION} CACHE INTERNAL "libApp version" FORCE) # Needed for cascaded version identification

include(ec_doxygen)        # Doxygen target doc
include(ec_compiler_presets)

set(CMAKE_CXX_STANDARD 11)

link_libraries("-Wl,--disable-new-dtags")
add_compile_definitions(_${CMAKE_SYSTEM_NAME}_ _GNU_SOURCE)

option(WITH_OMPI "Compile with OpenMP/MPI support" TRUE)
if (WITH_OMPI)
   find_package(MPI REQUIRED)
   find_package(OpenMP REQUIRED)
endif()

ec_build_info()

add_subdirectory(src/lib)
add_subdirectory(src/utils)
add_subdirectory(script)
add_subdirectory(test)

#----- Generate the config file for the project to be usable via cmake's find_package command
set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR     lib)
set(CONFIG_INSTALL_DIR  "${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CONFIG_INSTALL_DIR}"
    PATH_VARS           INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(EXPORT ${PROJECT_NAME}-shared-targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CONFIG_INSTALL_DIR})

install(EXPORT ${PROJECT_NAME}-static-targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CONFIG_INSTALL_DIR})

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CONFIG_INSTALL_DIR}"
)

if (EC_INIT_DONE LESS 2)
    # Packaging with CPack
    ec_package_name()                # Define package prefix  
    ec_build_config()                # Create build configuration script
    ec_prepare_ssm()                 # Prepare ssm packaging files

    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_VENDOR "ECCC")
    set(CPACK_PACKAGE_CONTACT "${MAINTAINER}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/package")
    set(CPACK_PACKAGE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}")
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "${NAME}_${PROJECT_VERSION}")
    include(CPack)
endif()