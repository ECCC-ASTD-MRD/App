
program mpmd_2
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_234, comm_124, comm_24
    integer :: return_status

    call App_MPMD_init(APP_MPMD_TEST2_ID)
    ! call mpmd_end_test()
    ! stop

    call validate_comm_size(App_MPMD_GetOwnComm(), NUM_PROCS_TEST2, '(2)')

    if (.not. (App_MPMD_HasComponent(APP_MPMD_TEST1_ID))) then
        print *, 'ERROR MPMD_2 can only be launched if MPMD_1 is present!'
        error stop 1
    end if

    comm_12 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST1_ID])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    ! Get it again, with inverted IDs. This should *not* be a collective call (mpmd_1 does not do it)
    comm_12 = App_MPMD_GetSharedComm([APP_MPMD_TEST1_ID, APP_MPMD_TEST2_ID])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    if (App_MPMD_HasComponent(APP_MPMD_TEST3_ID)) then
        comm_123 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST3_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST2_ID])
        call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(2, 1, 3)')
    end if

    if (App_MPMD_HasComponent(APP_MPMD_TEST3_ID) .and. App_MPMD_HasComponent(APP_MPMD_TEST4_ID)) then
        comm_234 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST4_ID, APP_MPMD_TEST3_ID])
        call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(2, 3, 4)')
    end if

    if (App_MPMD_HasComponent(APP_MPMD_TEST4_ID)) then
        comm_124 = App_MPMD_GetSharedComm([APP_MPMD_TEST4_ID, APP_MPMD_TEST1_ID, APP_MPMD_TEST2_ID])
        call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 1, 4)')

        comm_24 = App_MPMD_GetSharedComm([APP_MPMD_TEST2_ID, APP_MPMD_TEST4_ID])
        call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 4)')
    end if

    call mpmd_end_test()

end program mpmd_2
