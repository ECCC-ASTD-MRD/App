
program mpmd_3
    use mpi
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_12, comm_123, comm_13, comm_234, comm_134, comm_34
    integer :: mpmd_1id, mpmd_2id, mpmd_3id, mpmd_4id
    character(32) :: str

    call App_MPMD_init('mpmd_3', '0.0.0', MPI_THREAD_SINGLE)
    ! call mpmd_end_mpmd_()
    ! stop

    mpmd_1id = App_MPMD_GetComponentId('mpmd_1')
    mpmd_2id = App_MPMD_GetComponentId('mpmd_2')
    mpmd_3id = App_MPMD_GetSelfComponentId()
    mpmd_4id = App_MPMD_GetComponentId('mpmd_4')

    write(str, '(a,i1,a)') '(', mpmd_3id, ')'
    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST3, str)

    if (.not. (App_MPMD_HasComponent('mpmd_1') .and. App_MPMD_HasComponent('mpmd_2'))) then
        print *, 'ERROR MPMD_3 can only be launched if MPMD_1 and MPMD_2 are present!'
        error stop 1
    end if

    call App_Log(APP_WARNING, 'Expecting failure to retrieve shared comm')
    comm_12 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_1id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_2id, ', ', mpmd_1id, ')'
    call validate_comm_size(comm_12, 0, str)

    comm_123 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_3id, mpmd_1id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_2id, ', ', mpmd_3id, ', ', mpmd_1id, ')'
    call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, str)

    comm_13 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_3id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_1id, ', ', mpmd_3id, ')'
    call validate_comm_size(comm_13, NUM_PROCS_TEST1 + NUM_PROCS_TEST3, str)

    if (App_MPMD_HasComponent('mpmd_4')) then
        comm_234 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_4id, mpmd_2id])
        write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_3id, ', ', mpmd_4id, ', ', mpmd_2id, ')'
        call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)
    end if

    if (App_MPMD_HasComponent('mpmd_4')) then
        comm_134 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_3id])
        write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_1id, ', ', mpmd_3id, ')'
        call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)

        comm_34 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_4id])
        write(str, '(a,i1,a,i1,a)') '(', mpmd_3id, ', ', mpmd_4id, ')'
        call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)
    end if

    call mpmd_end_test()

end program mpmd_3

