message(STATUS "Generating App library")

file(GLOB PROJECT_INCLUDE_FILES *.h)
file(GLOB PROJECT_C_FILES *.c)
file(GLOB PROJECT_F_FILES *.f90)

#----- Common target dependencies
foreach(target IN ITEMS App App-ompi)
   add_library(${target} STATIC ${PROJECT_INCLUDE_FILES} ${PROJECT_C_FILES} ${PROJECT_F_FILES})
   
   message(STATUS "Adding dep to ${target} (${CMAKE_CURRENT_SOURCE_DIR})")
   add_dependencies(${target} ${PROJECT_NAME}_build_info)
   target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
   if(rmn_FOUND)
      target_link_libraries(${target} PRIVATE rmn)
   endif()
endforeach()

set_target_properties(App PROPERTIES 
   VERSION ${PROJECT_VERSION}
   PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
   Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
   POSITION_INDEPENDENT_CODE  ON)

set_target_properties(App-ompi PROPERTIES 
   VERSION ${PROJECT_VERSION}
   PUBLIC_HEADER "${PROJECT_INCLUDE_FILES}"
   Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
   POSITION_INDEPENDENT_CODE  ON)

#----- Specific target dependencies
target_compile_definitions(App-ompi PUBLIC HAVE_MPI HAVE_OPENMP)
target_link_libraries(App-ompi PUBLIC MPI::MPI_C OpenMP::OpenMP_C)

install(TARGETS App App-ompi 
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
   PUBLIC_HEADER DESTINATION include)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/app.mod TYPE INCLUDE)
