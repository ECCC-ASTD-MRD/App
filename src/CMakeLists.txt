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

# Non ompi version
set(targets App App-shared)
set(shared_targets App-shared)
# set(static_targets App-static)

add_library(App-obj OBJECT ${PROJECT_C_FILES} ${PROJECT_F_FILES})
target_include_directories(App-obj PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
)
target_include_directories(App-obj INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
target_link_libraries(App-obj m)
add_dependencies(App-obj ${PROJECT_NAME}_build_info)
set_target_properties(App-obj PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
    INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
    Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include
    POSITION_INDEPENDENT_CODE ON)

foreach(LINK_TYPE shared static)
    string(TOUPPER ${LINK_TYPE} LINK_TYPE_UP)
    add_library(App-${LINK_TYPE} ${LINK_TYPE_UP} $<TARGET_OBJECTS:App-obj>)
    target_link_libraries(App-${LINK_TYPE} App-obj)
    set_target_properties(App-${LINK_TYPE} PROPERTIES
        OUTPUT_NAME App)
endforeach(LINK_TYPE shared static)

# Needed when used as sbumodule/subtree
add_library(App::App-shared ALIAS App-shared)
# add_library(App::App-static ALIAS App-static)
add_library(App::App ALIAS App-shared)

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
    list(APPEND targets App-ompi-shared)
    # list(APPEND shared_targets App-ompi-shared)
    list(APPEND static_targets App-ompi-static)

    add_library(App-ompi-obj OBJECT ${PROJECT_C_FILES} ${PROJECT_F_FILES})
    add_dependencies(App-ompi-obj ${PROJECT_NAME}_build_info)
    target_include_directories(App-ompi-obj PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/atomic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared_memory>
    )
    target_include_directories(App-ompi-obj INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/ompi>)
    target_compile_definitions(App-ompi-obj PUBLIC HAVE_MPI HAVE_OPENMP)
    target_link_libraries(App-ompi-obj PUBLIC MPI::MPI_C MPI::MPI_Fortran OpenMP::OpenMP_C OpenMP::OpenMP_Fortran)
    target_compile_options(App-ompi-obj PUBLIC
        $<$<COMPILE_LANG_AND_ID:C,IntelLLVM>:-Wall -fno-omit-frame-pointer>
        $<$<COMPILE_LANG_AND_ID:Fortran,IntelLLVM>:-Wall fno-omit-frame-pointer>
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wall -fno-omit-frame-pointer>
        $<$<COMPILE_LANG_AND_ID:Fortran,GNU>:-Wall -fno-omit-frame-pointer>
    )

    set_target_properties(App-ompi-obj PROPERTIES
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
        INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
        Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ompi
        OUTPUT_NAME App-ompi
        POSITION_INDEPENDENT_CODE ON)

    foreach(LINK_TYPE static)
        string(TOUPPER ${LINK_TYPE} LINK_TYPE_UP)
        add_library(App-ompi-${LINK_TYPE} ${LINK_TYPE_UP} $<TARGET_OBJECTS:App-ompi-obj>)
        target_link_libraries(App-ompi-${LINK_TYPE} App-ompi-obj)
    endforeach()

    #----- Needed when used as sbumodule/subtree
    # add_library(App::App-ompi-shared ALIAS App-ompi-shared)
    add_library(App::App-ompi-static ALIAS App-ompi-static)
    add_library(App::App-ompi ALIAS App-ompi-static)
endif()

# install(TARGETS ${shared_targets}
#     EXPORT ${PROJECT_NAME}-shared-targets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     INCLUDES DESTINATION include
#     PUBLIC_HEADER DESTINATION include)

# install(TARGETS ${static_targets}
#     EXPORT ${PROJECT_NAME}-static-targets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     INCLUDES DESTINATION include
#     PUBLIC_HEADER DESTINATION include)  

install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/include/app.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/app_atomic_module.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/app_mutex_module.mod
    ${CMAKE_CURRENT_BINARY_DIR}/include/app_timer_module.mod
    TYPE INCLUDE)

if (TARGET App::App-ompi)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/ompi/app_mpmd.mod
        ${CMAKE_CURRENT_BINARY_DIR}/include/ompi/app_shared_memory_module.mod
        TYPE INCLUDE)
endif()
