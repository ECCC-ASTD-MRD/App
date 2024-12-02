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
   str.c)
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
target_include_directories(App-static PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(App-static INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

add_library(App-shared SHARED $<TARGET_OBJECTS:App-static>)
target_include_directories(App-shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(App-shared INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
 
add_dependencies(App-static ${PROJECT_NAME}_build_info)
set_target_properties(App-static App-shared PROPERTIES
   VERSION ${PROJECT_VERSION}
   PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
   INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
   Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include
   OUTPUT_NAME App
   POSITION_INDEPENDENT_CODE  ON)

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
   target_include_directories(App-ompi-static PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
   target_include_directories(App-ompi-static INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/ompi>)
   target_compile_definitions(App-ompi-static PUBLIC HAVE_MPI HAVE_OPENMP)
   target_link_libraries(App-ompi-static PUBLIC MPI::MPI_C MPI::MPI_Fortran OpenMP::OpenMP_C OpenMP::OpenMP_Fortran)

   add_library(App-ompi-shared SHARED $<TARGET_OBJECTS:App-ompi-static>)
   target_include_directories(App-ompi-shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
   target_include_directories(App-ompi-shared INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/ompi>)
   target_compile_definitions(App-ompi-shared PUBLIC HAVE_MPI HAVE_OPENMP)
   target_link_libraries(App-ompi-shared PUBLIC MPI::MPI_C MPI::MPI_Fortran OpenMP::OpenMP_C OpenMP::OpenMP_Fortran)

   set_target_properties(App-ompi-static App-ompi-shared PROPERTIES
      VERSION ${PROJECT_VERSION}
      PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
      INTERFACE_COMPILE_DEFINITIONS HAVE_APP="${PROJECT_VERSION}"
      Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ompi
      OUTPUT_NAME App-ompi
      POSITION_INDEPENDENT_CODE  ON)

   #----- Needed when used as sbumodule/subtree
   add_library(App::App-ompi-shared ALIAS App-ompi-shared)
   add_library(App::App-ompi-static ALIAS App-ompi-static)
   add_library(App::App-ompi ALIAS App-ompi-static) # Default to static
endif()

foreach(APP_TARGET ${shared_targets} ${static_targets})
   target_compile_options(
      ${APP_TARGET}
      PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wall -Wextra>
      $<$<COMPILE_LANG_AND_ID:C,GNU>:-Werror -Wno-error=pedantic -Wno-error=unused-function>
      $<$<COMPILE_LANG_AND_ID:Fortran,GNU>:-Wall -Wextra -fmax-errors=8>
      $<$<COMPILE_LANG_AND_ID:Fortran,GNU>:
               -Werror -Wno-error=unused-function -Wno-error=maybe-uninitialized -Wno-error=unused-variable
               -Wno-error=unused-dummy-argument -Wno-integer-division -Wno-error=c-binding-type>
      $<$<COMPILE_LANG_AND_ID:C,Intel>:-Wall -Werror>
      $<$<COMPILE_LANG_AND_ID:Fortran,Intel>:-error-limit 8 -warn all -diag-error=6188,6192 -diag-warning=7712>
   )
endforeach()

# Disable "unrecognized OpenMP pragma" for non-OpenMP builds
foreach(APP_TARGET App-static App-shared)
   target_compile_options(${APP_TARGET} PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,Intel>:-diag-disable=3180>
      $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-unknown-pragmas>
   )
endforeach()

install(TARGETS ${shared_targets}
   EXPORT ${PROJECT_NAME}-shared-targets
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
   INCLUDES DESTINATION include
   PUBLIC_HEADER DESTINATION include)

install(TARGETS ${static_targets}
   EXPORT ${PROJECT_NAME}-static-targets
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
   INCLUDES DESTINATION include
   PUBLIC_HEADER DESTINATION include)  

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

