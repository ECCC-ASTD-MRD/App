
program set_comm
    use mpi
    use app
    implicit none

    integer :: comm
    integer :: ierror

    app_ptr = App_Init(0, "set_comm", "test", "comm setting test", "now")

    call MPI_INIT(ierror)
    call App_Start()

    comm=MPI_COMM_WORLD
    call app_setmpicomm(comm)
 
    app_status=app_end(0)
    call MPI_Finalize(ierror)

end program set_comm
