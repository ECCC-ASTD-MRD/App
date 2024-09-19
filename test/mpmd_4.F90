
program mpmd_4
    use mpi
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_14, comm_234, comm_124, comm_134, comm_24, comm_34
    integer :: return_status

    call App_MPMD_init(APP_MPMD_TEST4_ID)

    call validate_comm_size(App_MPMD_GetOwnComm(), NUM_PROCS_TEST4, '(4)')

    if (.not. (App_MPMD_HasComponent(APP_MPMD_TEST1_ID) .and. App_MPMD_HasComponent(APP_MPMD_TEST2_ID) &
               .and. App_MPMD_HasComponent(APP_MPMD_TEST3_ID))) then
        print *, 'ERROR MPMD_4 can only be launched if MPMD_1, MPMD_2 and MPMD_3 are present!'
        error stop 1
    end if

    comm_14 = App_MPMD_GetSharedComm([APP_MPMD_TEST1_ID, APP_MPMD_TEST4_ID])
    call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, '(4, 1)')

    comm_234 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST3_ID, APP_MPMD_TEST4_ID])
    call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 2, 3)')

    comm_124 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST2_ID])
    call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(4, 1, 2)')

    comm_134 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST3_ID])
    call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 1, 3)')

    comm_24 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST4_ID])
    call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(4, 2)')

    comm_34 = App_MPMD_GetSharedComm([APP_MPMD_TEST3_ID, APP_MPMD_TEST4_ID])
    call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 3)')

    ! Set list should have been doubled, so check if the comms are still there (not collective)
    comm_14 = MPI_COMM_NULL
    comm_14 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST1_ID])
    call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, '(4, 1)')
    comm_234 = MPI_COMM_NULL
    comm_234 = App_MPMD_GetSharedComm([APP_MPMD_TEST3_ID, APP_MPMD_TEST4_ID, APP_MPMD_TEST2_ID])
    call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 2, 3)')
    comm_124 = MPI_COMM_NULL
    comm_124 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST2_ID, APP_MPMD_TEST1_ID])
    call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(4, 1, 2)')
    comm_134 = MPI_COMM_NULL
    comm_134 = App_MPMD_GetSharedComm([APP_MPMD_TEST3_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST4_ID])
    call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 1, 3)')
    comm_24 = MPI_COMM_NULL
    comm_24 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST2_ID])
    call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(4, 2)')
    comm_34 = MPI_COMM_NULL
    comm_34 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST3_ID])
    call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(4, 3)')

    call mpmd_end_test()

end program mpmd_4
