
program mpmd_2
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_234, comm_124, comm_24
    integer :: test1id, test2id, test3id, test4id, test5id

    call App_MPMD_init('test2', '0.0.0')
    ! call mpmd_end_test()
    ! stop

    test1id = App_MPMD_GetComponentId('test1')
    test2id = App_MPMD_GetSelfComponentId()
    test3id = App_MPMD_GetComponentId('test3')
    test4id = App_MPMD_GetComponentId('test4')
    test5id = App_MPMD_GetComponentId('test5')

    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST2, '(2)')

    if (.not. (App_MPMD_HasComponent('test1'))) then
        print *, 'ERROR MPMD_2 can only be launched if MPMD_1 is present!'
        error stop 1
    end if

    comm_12 = App_MPMD_GetSharedComm([test2id, test1id])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    ! Get it again, with inverted IDs. This should *not* be a collective call (mpmd_1 does not do it)
    comm_12 = App_MPMD_GetSharedComm([test1id, test2id])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    if (App_MPMD_HasComponent('test3id')) then
        comm_123 = App_MPMD_GetSharedComm([test2id, test3id, test1id, test1id, test2id])
        call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(2, 1, 3)')
    end if

    if (App_MPMD_HasComponent('test3id') .and. App_MPMD_HasComponent('test4id')) then
        comm_234 = App_MPMD_GetSharedComm([test2id, test4id, test3id])
        call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(2, 3, 4)')
    end if

    if (App_MPMD_HasComponent('test4id')) then
        comm_124 = App_MPMD_GetSharedComm([test4id, test1id, test2id])
        call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 1, 4)')

        comm_24 = App_MPMD_GetSharedComm([test2id, test4id])
        call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 4)')
    end if

    call mpmd_end_test()

end program mpmd_2
