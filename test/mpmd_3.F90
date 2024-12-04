
program mpmd_3
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_13, comm_234, comm_134, comm_34
    integer :: test1id, test2id, test3id, test4id

    call App_MPMD_init('test3', '0.0.0')
    ! call mpmd_end_test()
    ! stop

    test1id = App_MPMD_GetSelfComponentId()
    test2id = App_MPMD_GetComponentId('test2')
    test3id = App_MPMD_GetComponentId('test3')
    test4id = App_MPMD_GetComponentId('test4')

    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST3, '(3)')

    if (.not. (App_MPMD_HasComponent('test1') .and. App_MPMD_HasComponent('test2'))) then
        print *, 'ERROR MPMD_3 can only be launched if MPMD_1 and MPMD_2 are present!'
        error stop 1
    end if

    call App_Log(APP_WARNING, 'Expecting failure to retrieve shared comm')
    comm_12 = App_MPMD_GetSharedComm([test2id, test1id])
    call validate_comm_size(comm_12, 0, '(3)')

    comm_123 = App_MPMD_GetSharedComm([test2id, test3id, test1id])
    call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(3, 1, 2)')

    comm_13 = App_MPMD_GetSharedComm([test1id, test3id])
    call validate_comm_size(comm_13, NUM_PROCS_TEST1 + NUM_PROCS_TEST3, '(3, 1)')

    if (App_MPMD_HasComponent('test4')) then
        comm_234 = App_MPMD_GetSharedComm([test3id, test4id, test2id])
        call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(3, 2, 4)')
    end if

    if (App_MPMD_HasComponent('test4')) then
        comm_134 = App_MPMD_GetSharedComm([test4id, test1id, test3id])
        call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(3, 1, 4)')

        comm_34 = App_MPMD_GetSharedComm([test3id, test4id])
        call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(3, 4)')
    end if

    call mpmd_end_test()

end program mpmd_3

