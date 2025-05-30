IF(DEFINED ENV{INTELCOMP_HOME})
    add_link_options(-Wl,-rpath $ENV{INTELCOMP_HOME}/lib/intel64_lin) 
    add_link_options(-lintlc -lifcore -lifport)
endif()

add_executable(app app.c)
target_link_libraries(app App::App-ompi)

install(TARGETS app RUNTIME DESTINATION bin)
