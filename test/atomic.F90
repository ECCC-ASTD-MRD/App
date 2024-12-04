module atomic_test_module
  use App
  use iso_c_binding
  use mpi
  implicit none

  integer(MPI_ADDRESS_KIND), parameter :: NUM_SHARED_MEM_BYTES = 8
  integer, parameter :: NUM_ITERATIONS = 1000

contains

subroutine test_atomic_int32_add(atomic_var, rank, size)
  implicit none
  type(atomic_int32), intent(inout) :: atomic_var
  integer, intent(in) :: rank, size

  integer :: i, ierr
  integer :: new_val, expected_val
  logical :: success

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  if (rank == 0) then
    success = atomic_var % try_update(atomic_var % read(), 0)
    if (.not. success) then
      call App_Log(APP_ERROR, 'Should have been able to update the value!!!')
      error stop 1
    end if
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  do i = 1, NUM_ITERATIONS
    new_val = atomic_var % add(rank)
  end do

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  expected_val = ((size * (size-1)) / 2) * NUM_ITERATIONS

  if (atomic_var % read() /= expected_val) then
    write(App_msg, *) 'Atomic add is not working!!!', atomic_var % read(), expected_val
    call App_Log(APP_ERROR, app_msg)
    error stop 1
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

end subroutine test_atomic_int32_add

subroutine test_atomic_int32_try_update(atomic_var, rank, size)
  implicit none
  type(atomic_int32), intent(inout) :: atomic_var
  integer, intent(in) :: rank, size

  integer :: ierr, i
  integer(C_INT32_T) :: old_val, local_sum, global_sum
  logical :: success

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  if (rank == 0) then
    old_val = atomic_var % read()
    success = atomic_var % try_update(old_val, 0)
    if (.not. success) then
      call app_log(APP_ERROR, 'Failed')
      error stop 1
    end if
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  if (atomic_var % read() .ne. 0) then
    call app_log(APP_ERROR, 'Failed')
    error stop 1
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  ! Add, only trying once
  local_sum = 0
  do i = 1, NUM_ITERATIONS
    old_val = atomic_var % read()
    if (atomic_var % try_update(old_val, old_val + 1)) local_sum = local_sum + 1
  end do

  call MPI_Reduce(local_sum, global_sum, 1, MPI_INTEGER, MPI_SUM, 0, MPI_COMM_WORLD, ierr)

  if (rank == 0) then
    if (atomic_var % read() .ne. global_sum) then
      write(app_msg, *) 'Got the wrong result', global_sum, atomic_var % read()
      call App_Log(APP_FATAL, app_msg)
      error stop 1
    end if

    success = atomic_var % try_update(global_sum, 0)
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  ! Add, until we succeed
  do i = 1, NUM_ITERATIONS
    success = .false.
    do while (.not. success)
      old_val = atomic_var % read()
      success = atomic_var % try_update(old_val, old_val + 1)
    end do
  end do

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  if (atomic_var % read() .ne. NUM_ITERATIONS * size) then
    call App_Log(APP_ERROR, 'Wrong final sum')
    error stop 1
  end if


end subroutine test_atomic_int32_try_update

end module atomic_test_module

program atomic_test
  use atomic_test_module
  implicit none

  integer :: rank, size
  integer :: ierr, i

  type(C_PTR) :: cb_memory
  integer, pointer :: int_ptr
  type(atomic_int32) :: atomic_int

  call MPI_Init(ierr)
  call MPI_Comm_rank(MPI_COMM_WORLD, rank, ierr)
  call MPI_Comm_size(MPI_COMM_WORLD, size, ierr)
  
  if (size < 12) then
    write(app_msg, *) 'We want at least 12 processes for this test. We only have ', size
    call App_Log(APP_FATAL, app_msg)
    error stop 1
  end if

  cb_memory = app_allocate_shared(NUM_SHARED_MEM_BYTES, MPI_COMM_WORLD)

  call c_f_pointer(cb_memory, int_ptr)
  if (rank == 0) int_ptr = 0

  call atomic_int % init_from_int(int_ptr)

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  ! Test without atomic first
  do i = 1, NUM_ITERATIONS
    int_ptr = int_ptr + 1
  end do

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  if (int_ptr .ne. NUM_ITERATIONS * size) then
    write(app_msg, *) 'Result is wrong (as expected)!', int_ptr, NUM_ITERATIONS * size
    call App_Log(APP_INFO, app_msg)
  end if

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  call test_atomic_int32_try_update(atomic_int, rank, size)

  !-------------------------------------
  call MPI_Barrier(MPI_COMM_WORLD, ierr)
  !-------------------------------------

  call MPI_Finalize(ierr)

  call App_Log(APP_INFO, 'Test passed')

end program atomic_test
