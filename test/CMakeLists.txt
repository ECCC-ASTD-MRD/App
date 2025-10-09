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

        add_test(NAME app COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG} -n 1 $<TARGET_FILE:app> -s 10)

        target_link_libraries(app App::App-ompi)
        add_dependencies(check app)

        add_executable(test_app_loglevel_noinit EXCLUDE_FROM_ALL test_app_loglevel_noinit.F90)
        add_test(
            NAME test_app_loglevel_noinit
            COMMAND $<TARGET_FILE:test_app_loglevel_noinit>
        )
        target_link_libraries(test_app_loglevel_noinit App::App)
        add_dependencies(check test_app_loglevel_noinit)

        if (TARGET App::App-ompi)
            add_executable(atomic EXCLUDE_FROM_ALL atomic.F90)
            target_link_libraries(atomic App::App-ompi) # The test uses MPI, even if it is not necessary for atomic in general
            add_test(NAME atomic COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG} -n 12 $<TARGET_FILE:atomic>)
            add_dependencies(check atomic)

            add_executable(mutex EXCLUDE_FROM_ALL mutex.F90)
            target_link_libraries(mutex App::App-ompi) # The test uses MPI, even if it is not necessary for app_mutex in general
            add_test(NAME mutex COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG} -n 6 $<TARGET_FILE:mutex>)
            add_dependencies(check mutex)

            add_executable(multithread_noinit0 EXCLUDE_FROM_ALL multithread_noinit0.c)
            target_link_libraries(multithread_noinit0 App::App-ompi)
            add_test(NAME multithread_noinit0 COMMAND $<TARGET_FILE:multithread_noinit0>)
            add_dependencies(check multithread_noinit0)

            add_executable(init1 EXCLUDE_FROM_ALL init1.c)
            target_link_libraries(init1 App::App-ompi)
            add_dependencies(check init1)
            add_executable(init2 EXCLUDE_FROM_ALL init2.c)
            target_link_libraries(init2 App::App-ompi)
            add_dependencies(check init2)

            add_executable(set_comm EXCLUDE_FROM_ALL set_comm.F90)
            target_link_libraries(set_comm App::App-ompi)
            add_dependencies(check set_comm)

            add_library(mpmd_helper EXCLUDE_FROM_ALL mpmd_helper.F90)
            target_link_libraries(mpmd_helper App::App-ompi)
            add_executable(mpmd_1 EXCLUDE_FROM_ALL mpmd_1.F90)
            target_link_libraries(mpmd_1 App::App-ompi mpmd_helper)
            add_dependencies(check mpmd_1)
            add_executable(mpmd_2 EXCLUDE_FROM_ALL mpmd_2.F90)
            target_link_libraries(mpmd_2 App::App-ompi mpmd_helper)
            add_dependencies(check mpmd_2)
            add_executable(mpmd_3 EXCLUDE_FROM_ALL mpmd_3.F90)
            target_link_libraries(mpmd_3 App::App-ompi mpmd_helper)
            add_dependencies(check mpmd_3)
            add_executable(mpmd_4 EXCLUDE_FROM_ALL mpmd_4.F90)
            target_link_libraries(mpmd_4 App::App-ompi mpmd_helper)
            add_dependencies(check mpmd_4)
            add_executable(mpmd_5 EXCLUDE_FROM_ALL mpmd_5.c)
            target_link_libraries(mpmd_5 App::App-ompi)
            add_dependencies(check mpmd_5)

            add_executable(same_host EXCLUDE_FROM_ALL same_host.F90)
            target_link_libraries(same_host App::App-ompi)
            add_dependencies(check same_host)

            add_test(NAME App_MPMD_Init COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                -n 2 $<TARGET_FILE:init1> :
                -n 4 $<TARGET_FILE:init2>
            )
            add_test(NAME set_comm COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                -n 2 $<TARGET_FILE:set_comm>
            )
            add_test(NAME mpmd1 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                -n 1 $<TARGET_FILE:mpmd_1>
            )
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
                    -n 16 $<TARGET_FILE:mpmd_4> :
                    -n  4 $<TARGET_FILE:mpmd_2> :
                    -n  1 $<TARGET_FILE:mpmd_1>
            )
            add_test(NAME mpmd10 COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                    -n 1 $<TARGET_FILE:mpmd_1> :
                    -n 5 $<TARGET_FILE:mpmd_5>
            )

            add_test(NAME same_host COMMAND ${MPIEXEC_EXECUTABLE} ${OVERSUBSCRIBE_FLAG}
                -n 1 $<TARGET_FILE:same_host> 1
            )
        endif()
    endif()
endif()
