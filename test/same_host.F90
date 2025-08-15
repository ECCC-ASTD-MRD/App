program same_host
    use mpi
    use App, only: App_samehost, App_log, APP_ERROR, APP_INFO
    implicit none

    integer :: ierr
    integer :: expected_int
    character(len = 32) :: arg
    logical :: expected_samehost

    call get_command_argument(1, arg)
    if (len_trim(arg) == 0) error stop
    read(arg, '(i1)', iostat = ierr) expected_int
    if (ierr /= 0) then
        call App_Log(APP_ERROR, 'Could not parse command line argument for expected same_host')
        error stop
    endif
    expected_samehost = expected_int == 1

    call MPI_Init(ierr)

    if (App_samehost(MPI_COMM_WORLD) .neqv. expected_samehost) then
        call App_Log(APP_ERROR, 'Did not get the expected value from App_samehost!')
        call MPI_Finalize(ierr)
        error stop
    endif

    call MPI_Finalize(ierr)

    call App_Log(APP_INFO, 'Test passed')
end program