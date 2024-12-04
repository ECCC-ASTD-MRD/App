
program mpmd_1
    use app
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_13, comm_14, comm_124, comm_134
    integer :: comm_15
    integer :: test1id, test2id, test3id, test4id, test5id

    call App_MPMD_Init("test1", "0.0.0")
    ! call mpmd_end_test()
    ! stop

    test1id = App_MPMD_GetSelfComponentId()
    test2id = App_MPMD_GetComponentId('test2')
    test3id = App_MPMD_GetComponentId('test3')
    test4id = App_MPMD_GetComponentId('test4')
    test5id = App_MPMD_GetComponentId('test5')

    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST1, '(1)')

    if (App_MPMD_HasComponent("test2")) then
        comm_12 = App_MPMD_GetSharedComm([test1id, test2id])
        call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(1, 2)')
    end if

    if (App_MPMD_HasComponent("test2") .and. App_MPMD_HasComponent("test3")) then
        comm_123 = App_MPMD_GetSharedComm([test3id, test1id, test2id])
        call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(1, 2, 3)')
    end if

    if (App_MPMD_HasComponent("test3")) then
        comm_13 = App_MPMD_GetSharedComm([test1id, test3id])
        call validate_comm_size(comm_13, NUM_PROCS_TEST1 + NUM_PROCS_TEST3, '(1, 3)')
    end if

    if (App_MPMD_HasComponent("test4")) then
        comm_14 = App_MPMD_GetSharedComm([test1id, test4id])
        call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, '(1, 4)')
    end if

    if (App_MPMD_HasComponent("test2") .and. App_MPMD_HasComponent("test4")) then
        comm_124 = App_MPMD_GetSharedComm([test4id, test1id, test2id])
        call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(1, 2, 4)')
    end if

    if (App_MPMD_HasComponent("test3") .and. App_MPMD_HasComponent("test4")) then
        comm_134 = App_MPMD_GetSharedComm([test4id, test1id, test3id])
        call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(1, 3, 4)')
    end if

    if (App_MPMD_HasComponent("test5")) then
        comm_15 = App_MPMD_GetSharedComm([test5id, test1id])
        call validate_comm_size(comm_15, NUM_PROCS_TEST1 + NUM_PROCS_TEST5, '(1, 5)')
    end if

    call mpmd_end_test()

end program mpmd_1
