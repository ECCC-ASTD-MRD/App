
program mpmd_4
    use mpi
    use app_mpmd
    use app_test_mpmd_helper
    implicit none

    integer :: comm_14, comm_234, comm_124, comm_134, comm_24, comm_34
    integer :: mpmd_1id, mpmd_2id, mpmd_3id, mpmd_4id, mpmd_5id
    character(32) :: str
    integer :: ierror

    app_ptr=app_init(0,"mpmd_4","test", "mpmd context attempt","now")

    call MPI_INIT(ierror)
    call App_MPMD_Init()
    call App_Start()


    mpmd_1id = App_MPMD_GetComponentId('mpmd_1')
    mpmd_2id = App_MPMD_GetComponentId('mpmd_2')
    mpmd_3id = App_MPMD_GetComponentId('mpmd_3')
    mpmd_4id = App_MPMD_GetSelfComponentId()
    mpmd_5id = App_MPMD_GetComponentId('mpmd_5')

    write(str, '(a,i1,a)') '(', mpmd_4id, ')'
    call validate_comm_size(App_MPMD_GetSelfComm(), NUM_PROCS_TEST4, str)

    if (.not. (App_MPMD_HasComponent('mpmd_1') .and. App_MPMD_HasComponent('mpmd_2') &
               .and. App_MPMD_HasComponent('mpmd_3'))) then
        print *, 'ERROR MPMD_4 can only be launched if MPMD_1, MPMD_2 and MPMD_3 are present!'
        error stop 1
    end if

    comm_14 = App_MPMD_GetSharedComm([mpmd_1id, mpmd_4id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_1id, ')'
    call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, '(4, 1)')

    comm_234 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_3id, mpmd_4id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_2id, ', ', mpmd_3id, ', ', mpmd_4id, ')'
    call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)

    comm_124 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_2id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_1id, ', ', mpmd_2id, ')'
    call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, str)

    comm_134 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id, mpmd_3id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_1id, ', ', mpmd_3id, ')'
    call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)

    comm_24 = App_MPMD_GetSharedComm([mpmd_2id, mpmd_4id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_2id, ', ', mpmd_4id, ')'
    call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, str)

    comm_34 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_4id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_3id, ', ', mpmd_4id, ')'
    call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)

    ! Set list should have been doubled, so check if the comms are still there (not collective)
    comm_14 = MPI_COMM_NULL
    comm_14 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_1id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_1id, ')'
    call validate_comm_size(comm_14, NUM_PROCS_TEST1 + NUM_PROCS_TEST4, str)
    comm_234 = MPI_COMM_NULL
    comm_234 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_4id, mpmd_2id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_3id, ', ', mpmd_4id, ', ', mpmd_2id, ')'
    call validate_comm_size(comm_234, NUM_PROCS_TEST2 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)
    comm_124 = MPI_COMM_NULL
    comm_124 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_2id, mpmd_1id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_2id, ', ', mpmd_1id, ')'
    call validate_comm_size(comm_124, NUM_PROCS_TEST1 + NUM_PROCS_TEST2 + NUM_PROCS_TEST4, str)
    comm_134 = MPI_COMM_NULL
    comm_134 = App_MPMD_GetSharedComm([mpmd_3id, mpmd_1id, mpmd_4id])
    write(str, '(a,i1,a,i1,a,i1,a)') '(', mpmd_3id, ', ', mpmd_1id, ', ', mpmd_4id, ')'
    call validate_comm_size(comm_134, NUM_PROCS_TEST1 + NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)
    comm_24 = MPI_COMM_NULL
    comm_24 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_2id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_2id, ')'
    call validate_comm_size(comm_24, NUM_PROCS_TEST2 + NUM_PROCS_TEST4, str)
    comm_34 = MPI_COMM_NULL
    comm_34 = App_MPMD_GetSharedComm([mpmd_4id, mpmd_3id])
    write(str, '(a,i1,a,i1,a)') '(', mpmd_4id, ', ', mpmd_3id, ')'
    call validate_comm_size(comm_34, NUM_PROCS_TEST3 + NUM_PROCS_TEST4, str)

    call mpmd_end_test()

end program mpmd_4
