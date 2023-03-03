message(STATUS "Generating App library")

file(GLOB PROJECT_INCLUDE_FILES *.h)
file(GLOB PROJECT_C_FILES *.c)
file(GLOB PROJECT_F_FILES *.f90)

#----- Common target dependencies
foreach(target IN ITEMS App App-ompi)
   add_library(${target} STATIC ${PROJECT_INCLUDE_FILES} ${PROJECT_C_FILES} ${PROJECT_F_FILES})
   
   message(STATUS "Adding dep to ${target} (${CMAKE_CURRENT_SOURCE_DIR})")
   add_dependencies(${target} ${PROJECT_NAME}_build_info)
   target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
   if(rmn_FOUND)
      target_link_libraries(${target} PRIVATE rmn)
   endif()

   set(FORTRAN_MODULE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/${target})
   set_target_properties(${target} PROPERTIES
      VERSION ${PROJECT_VERSION}
      PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
      Fortran_MODULE_DIRECTORY ${FORTRAN_MODULE_DIR}
      POSITION_INDEPENDENT_CODE  ON)
   target_include_directories(${target} INTERFACE $<BUILD_INTERFACE:${FORTRAN_MODULE_DIR}>)
endforeach()

# TODO: Replace with target_compile_options(...) and target_link_options(...)
# target_compile_options(App-ompi PRIVATE ${MPI_COMPILE_FLAGS})
# target_link_options(App-ompi PRIVATE ${MPI_LINK_FLAGS})
set_target_properties(App-ompi PROPERTIES 
   COMPILE_FLAGS "${MPI_COMPILE_FLAGS}"
   LINK_FLAGS "${MPI_LINK_FLAGS}")

#----- Specific target dependencies
target_compile_definitions(App-ompi PUBLIC HAVE_MPI HAVE_OPENMP)
target_link_libraries(App-ompi PUBLIC MPI::MPI_C OpenMP::OpenMP_C)

install(TARGETS App App-ompi 
   EXPORT AppTargets
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
   PUBLIC_HEADER DESTINATION include
   INCLUDES DESTINATION)

install(EXPORT AppTargets
    NAMESPACE App::
    DESTINATION ${CONFIG_INSTALL_DIR})

# TODO: Have a separate module install path for each different targets
install(FILES ${FORTRAN_MODULE_DIR}/app.mod TYPE INCLUDE)
