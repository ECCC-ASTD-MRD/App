
program mpmd_1
    use mpi
    use app
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_13, comm_14, comm_124, comm_134
    integer :: comm_15
    integer :: mpmd_1id, mpmd_2id, mpmd_3id, mpmd_4id, mpmd_5id

    call App_MPMD_Init("mpmd_1", "0.0.0", MPI_THREAD_SINGLE)

    mpmd_1id = App_MPMD_GetSelfComponentId()
    mpmd_2id = App_MPMD_GetComponentId('mpmd_2')
    mpmd_3id = App_MPMD_GetComponentId('mpmd_3')
    mpmd_4id = App_MPMD_GetComponentId('mpmd_4')
    mpmd_5id = App_MPMD_GetComponentId('mpmd_5')

    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST1, '(1)')

    if (App_MPMD_HasComponent("mpmd_2")) then
        comm_12 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_2id])
        call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(1, 2)')
    end if

    if (App_MPMD_HasComponent("mpmd_2") .and. App_MPMD_HasComponent("mpmd_3")) then
        comm_123 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_1id, mpmd_2id])
        call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(1, 2, 3)')
    end if

    if (App_MPMD_HasComponent("mpmd_3")) then
        comm_13 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_3id])
        call validate_comm_size(comm_13, NUM_PROCS_TEST1 + NUM_PROCS_TEST3, '(1, 3)')
    end if

    if (App_MPMD_HasComponent("mpmd_4")) then
        comm_14 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_4id])
        call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, '(1, 4)')
    end if

    if (App_MPMD_HasComponent("mpmd_2") .and. App_MPMD_HasComponent("mpmd_4")) then
        comm_124 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_2id])
        call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(1, 2, 4)')
    end if

    if (App_MPMD_HasComponent("mpmd_3") .and. App_MPMD_HasComponent("mpmd_4")) then
        comm_134 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_3id])
        call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(1, 3, 4)')
    end if

    if (App_MPMD_HasComponent("mpmd_5")) then
        comm_15 = App_MPMD_GetSharedComm([mpmd_5id, mpmd_1id])
        call validate_comm_size(comm_15, NUM_PROCS_TEST1 + NUM_PROCS_TEST5, '(1, 5)')
    end if

    call mpmd_end_test()

end program mpmd_1
