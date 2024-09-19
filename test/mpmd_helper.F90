module app_test_mpmd_helper
    use app
    use app_mpmd
    use mpi
    implicit none
    private

    public :: validate_comm_size, mpmd_end_test

    integer, parameter, public :: NUM_PROCS_TEST1 = 1
    integer, parameter, public :: NUM_PROCS_TEST2 = 4
    integer, parameter, public :: NUM_PROCS_TEST3 = 9
    integer, parameter, public :: NUM_PROCS_TEST4 = 16
    integer, parameter, public :: NUM_PROCS_TEST5 = 5

contains

!> Check wether the given communicator has the expected number of processes.
!> If it does not, crash the program
subroutine validate_comm_size(comm, expected_num_procs, msg)
    implicit none
    integer, intent(in) :: comm                 !< Communicator we are checking
    integer, intent(in) :: expected_num_procs   !< How many processes it should have if we don't wanna crash
    character(len=*), intent(in) :: msg

    integer :: num_procs, ierr

    if (expected_num_procs == 0) then
        if (comm /= MPI_COMM_NULL) then
            call app_log(APP_ERROR, 'We were expecting a NULL communicator! ' // trim(msg))
            error stop 1
        end if
        return
    end if

    call MPI_Comm_size(comm, num_procs, ierr)

    if (num_procs /= expected_num_procs) then
        write(app_msg, '(A, I5, A, I5, 1X, A)') 'We have ', num_procs, ' PEs, but we should have ', expected_num_procs, msg
        call app_log(APP_ERROR, app_msg)
        error stop 1
    end if
end subroutine validate_comm_size

subroutine mpmd_end_test()
    implicit none
    integer :: ierr
    ! return_status = app_end(0)
    call App_MPMD_Finalize()
    ! call MPI_Finalize(ierr)
end subroutine mpmd_end_test

end module app_test_mpmd_helper
