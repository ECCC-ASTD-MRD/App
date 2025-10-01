message(STATUS "Generating App library")

set(PROJECT_INCLUDE_FILES
    App.h
    atomic/App_Atomic.h
    atomic/App_Atomic.inc
    App_Timer.h
    str.h
)
set(PROJECT_C_FILES
    App.c
    atomic/App_Atomic.c
    App_Timer.c
    str.c
)
set(PROJECT_F_FILES
    App.F90
    atomic/App_Atomic.F90
    App_Mutex.F90
    App_Timer.F90
)

#----- Non ompi version
set(targets App App-shared)
set(shared_targets App-shared)
set(static_targets App-static)

add_library(App-static STATIC ${PROJECT_INCLUDE_FILES} ${PROJECT_C_FILES} ${PROJECT_F_FILES})
target_include_directories(App-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
)
target_include_directories(App-static INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/fmod>
    $<INSTALL_INTERFACE:include/fmod>)

add_library(App-shared SHARED $<TARGET_OBJECTS:App-static>)
target_include_directories(App-shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
)
target_include_directories(App-shared INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/fmod>
    $<INSTALL_INTERFACE:include/fmod>)

add_dependencies(App-static ${PROJECT_NAME}_build_info)
set_target_properties(App-static App-shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
    INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
    Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/fmod
    OUTPUT_NAME App
    POSITION_INDEPENDENT_CODE ON
)

#----- Needed when used as sbumodule/subtree
add_library(App::App-shared ALIAS App-shared)
add_library(App::App-static ALIAS App-static)
add_library(App::App ALIAS App-static) # Default to static

if(WITH_OMPI AND OpenMP_FOUND AND MPI_FOUND)
    message(STATUS "Generating App ompi library")

    list(APPEND PROJECT_F_FILES
        App_MPMD_Module.F90
        shared_memory/App_Shared_Memory.F90
    )
    list(APPEND PROJECT_C_FILES
        App_MPMD.c
        shared_memory/App_Shared_Memory.c
    )
    list(APPEND PROJECT_INCLUDE_FILES
        App_MPMD.h
        shared_memory/App_Shared_Memory.h
        shared_memory/App_Shared_Memory.inc
    )

    #----- ompi version
    list(APPEND targets App-ompi-static App-ompi-shared)
    list(APPEND shared_targets App-ompi-shared)
    list(APPEND static_targets App-ompi-static)

    add_library(App-ompi-static STATIC ${PROJECT_INCLUDE_FILES} ${PROJECT_C_FILES} ${PROJECT_F_FILES})
    add_dependencies(App-ompi-static ${PROJECT_NAME}_build_info)
    target_include_directories(App-ompi-static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
    )
    target_include_directories(App-ompi-static INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/ompi>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi>
        $<INSTALL_INTERFACE:include/fmod-ompi>)
    target_compile_definitions(App-ompi-static PUBLIC HAVE_MPI HAVE_OPENMP)
    target_link_libraries(App-ompi-static PUBLIC MPI::MPI_C MPI::MPI_Fortran OpenMP::OpenMP_C OpenMP::OpenMP_Fortran)

    add_library(App-ompi-shared SHARED $<TARGET_OBJECTS:App-ompi-static>)
    target_include_directories(App-ompi-shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
    )
    target_include_directories(App-ompi-shared INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/ompi>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi>
        $<INSTALL_INTERFACE:include/fmod-ompi>)
    target_compile_definitions(App-ompi-shared PUBLIC HAVE_MPI HAVE_OPENMP)
    target_link_libraries(App-ompi-shared PUBLIC MPI::MPI_C MPI::MPI_Fortran OpenMP::OpenMP_C OpenMP::OpenMP_Fortran)

    set_target_properties(App-ompi-static App-ompi-shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
        INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
        Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi
        OUTPUT_NAME App-ompi
        POSITION_INDEPENDENT_CODE ON
    )

    #----- Needed when used as sbumodule/subtree
    add_library(App::App-ompi-shared ALIAS App-ompi-shared)
    add_library(App::App-ompi-static ALIAS App-ompi-static)
    add_library(App::App-ompi ALIAS App-ompi-static) # Default to static
endif()

install(TARGETS ${shared_targets}
    EXPORT ${PROJECT_NAME}-shared-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
    PUBLIC_HEADER DESTINATION include
)

install(TARGETS ${static_targets}
    EXPORT ${PROJECT_NAME}-static-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
    PUBLIC_HEADER DESTINATION include
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/include/fmod/app.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/fmod/app_atomic_module.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/fmod/app_mutex_module.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/fmod/app_timer_module.mod
    DESTINATION include/fmod
)

if (TARGET App::App-ompi)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app_atomic_module.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app_mutex_module.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app_timer_module.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app_mpmd.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/fmod-ompi/app_shared_memory_module.mod
        DESTINATION include/fmod-ompi
    )
endif()

