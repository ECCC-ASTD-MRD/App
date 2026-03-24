IF(DEFINED ENV{INTELCOMP_HOME})
    add_link_options(-Wl,-rpath $ENV{INTELCOMP_HOME}/lib/intel64_lin) 
    add_link_options(-lintlc -lifcore -lifport)
endif()

add_executable(app app.c)
if (TARGET App::App-ompi)
   target_link_libraries(app App::App-ompi)
else()   
   target_link_libraries(app App::App)
endif()

install(TARGETS app RUNTIME DESTINATION bin)
