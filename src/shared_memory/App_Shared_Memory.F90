
module App_Shared_Memory_Module
  use iso_c_binding
  implicit none
  private
  include 'App_Shared_Memory.inc'

  public :: app_allocate_shared
  public :: shmem_allocate_shared, shmem_address_from_id

  external :: MPI_Allgather, MPI_Bcast

contains

    !> Allocate one area of shared memory and distribute a (process-local) pointer to it to every process in
    !> the given communicator. This is a collective call on that communicator, so every process that belongs
    !> to it must do the call. Only the process with rank 0 will actually do the allocation, so only the size
    !> given to that process is taken into account.
    function app_allocate_shared(wsize, comm) result(shmem_ptr)
        use mpi
        implicit none
        INTEGER(KIND=MPI_ADDRESS_KIND), INTENT(IN) :: wsize !< Memory size (in bytes)
        integer,                        INTENT(IN) :: comm  !< Communicator whose process will access the shared memory
        type(C_PTR) :: shmem_ptr                            !< Process-local pointer to the allocated shared memory

        integer :: myrank, shmid, thesize, i, hostid
        integer :: ierr
        integer(C_INT64_T) :: siz
        type(C_PTR) :: p
        integer, dimension(:), allocatable :: hostids

        interface
            function c_get_hostid() result(id) bind(C,name='gethostid')
            import :: C_LONG
            implicit none
            integer(C_LONG) :: id
            end function c_get_hostid
        end interface

        shmem_ptr = C_NULL_PTR
        p = C_NULL_PTR
        shmem_ptr = transfer(p, shmem_ptr) ! ??

        call MPI_Comm_rank(comm, myrank, ierr)
        call MPI_Comm_size(comm, thesize, ierr)
        allocate(hostids(thesize))
        hostid = INT(c_get_hostid(), 4)
        call MPI_Allgather(hostid, 1, MPI_INTEGER, hostids, 1, MPI_INTEGER, comm, ierr)
        do i = 1, thesize
            if(hostids(i) .ne. hostid) return   ! ERROR, hostid MUST be the same everywhere
        enddo

        if(myrank == 0) then
            siz = wsize
            p = shmem_allocate_shared(shmid, siz)
        endif
        call MPI_Bcast(shmid, 1, MPI_INTEGER, 0, comm, ierr)
        if(myrank .ne. 0) then
            p = shmem_address_from_id(shmid)
        endif
        shmem_ptr = transfer(p, shmem_ptr)
    end function app_allocate_shared

end module App_Shared_Memory_Module
