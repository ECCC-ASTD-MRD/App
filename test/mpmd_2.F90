
program mpmd_2
    use app_mpmd
    use app_test_mpmd_helper
    use mpi_f08
    implicit none

    integer :: comm_12, comm_123, comm_234, comm_124, comm_24
    integer :: mpmd_1id, mpmd_2id, mpmd_3id, mpmd_4id
    integer :: ierror

    app_ptr=App_Init(0,"mpmd_2","test", "mpmd context attempt","now")

    call MPI_INIT(ierror)
    call App_MPMD_Init()
    call App_Start()

    ! call mpmd_end_mpmd_()
    ! stop

    mpmd_1id = App_MPMD_GetComponentId('mpmd_1')
    mpmd_2id = App_MPMD_GetSelfComponentId()
    mpmd_3id = App_MPMD_GetComponentId('mpmd_3')
    mpmd_4id = App_MPMD_GetComponentId('mpmd_4')

    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST2, '(2)')

    if (.not. (App_MPMD_HasComponent('mpmd_1'))) then
        print *, 'ERROR MPMD_2 can only be launched if MPMD_1 is present!'
        error stop 1
    end if

    comm_12 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_1id])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    ! Get it again, with inverted IDs. This should *not* be a collective call (mpmd_1 does not do it)
    comm_12 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_2id])
    call validate_comm_size(comm_12, NUM_PROCS_TEST1 + NUM_PROCS_TEST2, '(2, 1)')

    if (App_MPMD_HasComponent('mpmd_3')) then
        comm_123 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_3id, mpmd_1id, mpmd_1id, mpmd_2id])
        call validate_comm_size(comm_123, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST3, '(2, 1, 3)')
    end if

    if (App_MPMD_HasComponent('mpmd_3') .and. App_MPMD_HasComponent('mpmd_4')) then
        comm_234 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_4id, mpmd_3id])
        call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, '(2, 3, 4)')
    end if

    if (App_MPMD_HasComponent('mpmd_4')) then
        comm_124 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_2id])
        call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 1, 4)')

        comm_24 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_4id])
        call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, '(2, 4)')
    end if

    call mpmd_end_test()
    
end program mpmd_2
