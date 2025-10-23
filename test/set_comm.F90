
program set_comm
    use mpi
    use app
    implicit none

    integer :: comm
    integer :: ierror

    app_ptr = App_Init(0, "set_comm", "test", "comm setting test", "now")

    call MPI_INIT(ierror)
    call App_Start()

    call MPI_Barrier(MPI_COMM_WORLD,ierror);
    call app_log(APP_INFO,'setting communicator')
    comm=MPI_COMM_WORLD
    call app_setmpicomm(comm)

    call app_log(APP_INFO,'log to from default ranks')
    call app_logallranks(APP_INFO,'log to all ranks')
  
    app_status=app_end(0)
    call MPI_Finalize(ierror)

end program set_comm
