if (EC_INIT_DONE LESS 2)
   include(CTest)
   add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=true ${CMAKE_CTEST_COMMAND})

   if(BUILD_TESTING)
        # Check MPI implementation to be able to give the proper flags when running tests
        find_program(RPN_MPIRUN mpirun)
        if (${RPN_MPIRUN} STREQUAL "RPN_MPIRUN-NOTFOUND")
            message(FATAL_ERROR "mpirun not found. Won't be able to configure the version.")
        else()
            execute_process(
                COMMAND ${RPN_MPIRUN} --version 2>&1
                COMMAND grep -E -o "Open MPI"
                OUTPUT_VARIABLE MPI_IMPLEMENTATION
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if ("x${MPI_IMPLEMENTATION}" STREQUAL "xOpen MPI")
                set(OVERSUBSCRIBE_FLAG "-oversubscribe")
            endif()
            # message(WARNING "Oversubscribe flag: ${OVERSUBSCRIBE_FLAG}")
        endif()

        add_executable(test_app_loglevel_noinit EXCLUDE_FROM_ALL test_app_loglevel_noinit.F90)
        add_test(
            NAME test_app_loglevel_noinit
            COMMAND $<TARGET_FILE:test_app_loglevel_noinit>
        )
        set_tests_properties(test_app_loglevel_noinit
            PROPERTIES PASS_REGULAR_EXPRESSION "INFO")
        target_link_libraries(test_app_loglevel_noinit App::App)
        add_dependencies(check test_app_loglevel_noinit)

        if (TARGET App::App-ompi)
            add_executable(multithread_noinit0 multithread_noinit0.c)
            target_link_libraries(multithread_noinit0 App::App-ompi)
            add_test(NAME multithread_noinit0 COMMAND $<TARGET_FILE:multithread_noinit0>)
            add_dependencies(check multithread_noinit0)

            add_library(mpmd_helper mpmd_helper.F90)
            target_link_libraries(mpmd_helper App::App-ompi)
            add_executable(mpmd_1 mpmd_1.F90)
            target_link_libraries(mpmd_1 App::App-ompi mpmd_helper)
            add_executable(mpmd_2 mpmd_2.F90)
            target_link_libraries(mpmd_2 App::App-ompi mpmd_helper)
            add_executable(mpmd_3 mpmd_3.F90)
            target_link_libraries(mpmd_3 App::App-ompi mpmd_helper)
            add_executable(mpmd_4 mpmd_4.F90)
            target_link_libraries(mpmd_4 App::App-ompi mpmd_helper)
            add_executable(mpmd_5 mpmd_5.c)
            target_link_libraries(mpmd_5 App::App-ompi)

            add_test(NAME mpmd1 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG} -n 1 $<TARGET_FILE:mpmd_1>)
            add_test(NAME mpmd2 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 1 $<TARGET_FILE:mpmd_1> :
                    -n 4 $<TARGET_FILE:mpmd_2>
            )
            add_test(NAME mpmd3 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 4 $<TARGET_FILE:mpmd_2> :
                    -n 1 $<TARGET_FILE:mpmd_1>
            )
            add_test(NAME mpmd4 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 4 $<TARGET_FILE:mpmd_2> :
                    -n 1 $<TARGET_FILE:mpmd_1> :
                    -n 9 $<TARGET_FILE:mpmd_3>
            )
            add_test(NAME mpmd5 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 4 $<TARGET_FILE:mpmd_2> :
                    -n 9 $<TARGET_FILE:mpmd_3> :
                    -n 1 $<TARGET_FILE:mpmd_1>
            )
            add_test(NAME mpmd6 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n  4 $<TARGET_FILE:mpmd_2> :
                    -n  9 $<TARGET_FILE:mpmd_3> :
                    -n 16 $<TARGET_FILE:mpmd_4> :
                    -n  1 $<TARGET_FILE:mpmd_1>
            )
            add_test(NAME mpmd7 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n  9 $<TARGET_FILE:mpmd_3> :
                    -n 16 $<TARGET_FILE:mpmd_4> :
                    -n  4 $<TARGET_FILE:mpmd_2> :
                    -n  1 $<TARGET_FILE:mpmd_1>
            )
            add_test(NAME mpmd8 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n  1 $<TARGET_FILE:mpmd_1> :
                    -n  4 $<TARGET_FILE:mpmd_2> :
                    -n  9 $<TARGET_FILE:mpmd_3> :
                    -n 16 $<TARGET_FILE:mpmd_4>
            )
            add_test(NAME mpmd9 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n  9 $<TARGET_FILE:mpmd_3> :
                    -n 11 $<TARGET_FILE:mpmd_4> :
                    -n  4 $<TARGET_FILE:mpmd_2> :
                    -n  1 $<TARGET_FILE:mpmd_1> :
                    -n  5 $<TARGET_FILE:mpmd_4>
            )
            add_test(NAME mpmd10 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 1 $<TARGET_FILE:mpmd_1> :
                    -n 5 $<TARGET_FILE:mpmd_5>
            )
            add_dependencies(check mpmd_1)
        endif()

    endif()
endif()
