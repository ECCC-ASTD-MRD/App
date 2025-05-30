
module App_Mutex_Module
    use iso_c_binding
    use App_Atomic_Module
    use App_Timer_Module
    implicit none

    private

    !> Mutex implementation. May be used across processes, if using shared memory
    type, public :: App_Mutex
        private
        
        type(C_PTR)     :: c_location = C_NULL_PTR  !< Pointer to the location of the mutex lock
        type(App_Timer) :: wait_timer               !< Timer to keep track of how long we have waited on this mutex

        !> Identifier for the mutex. When using multiple instances, only an instance with the proper ID can unlock a
        !> locked location
        integer(C_INT)  :: id = -1
    contains
        procedure, pass :: is_valid => AMM_is_valid
        procedure, pass :: init_from_ptr => AMM_init_from_ptr
        procedure, pass :: init_from_int => AMM_init_from_int
        procedure, pass :: lock => AMM_lock
        procedure, pass :: unlock => AMM_unlock
        procedure, pass :: try_lock => AMM_try_lock
        procedure, pass :: is_locked => AMM_is_locked
        procedure, pass :: is_locked_by_me => AMM_is_locked_by_me
        procedure, pass :: reset => AMM_reset
        procedure, pass :: get_id => AMM_get_id
        procedure, pass :: get_total_wait_time_ms => AMM_get_total_wait_time_ms
        procedure, pass :: get_latest_wait_time_ms => AMM_get_latest_wait_time_ms
    end type App_Mutex

contains
  
  !> Check whether the mutex has been initialized
  function AMM_is_valid(this) result(is_valid)
    implicit none
    class(App_Mutex), intent(in) :: this
    logical :: is_valid
    is_valid = c_associated(this % c_location) .and. (this % id >= 0)
  end function AMM_is_valid

  !> Initialize this mutex to use the given pointer as a location for its lock
  subroutine AMM_init_from_ptr(this, c_location, id)
    implicit none
    class(App_Mutex), intent(inout)     :: this       !< App_Mutex instance
    type(C_PTR),      intent(in), value :: c_location !< Pointer to a location for the lock
    integer(C_INT),   intent(in)        :: id         !< Identifier to give to this mutex

    if (.not. this % is_valid()) then
      this % c_location = c_location
      this % id = id
      call this % reset()
    end if
  end subroutine AMM_init_from_ptr

  !> Initialize this mutex to use the given integer (location) for its lock.
  !> If the mutex has already been initialize, this function does nothing
  subroutine AMM_init_from_int(this, lock_var, id)
    implicit none
    class(App_Mutex), intent(inout)      :: this      !< App_Mutex instance
    integer(C_INT),   intent(in), target :: lock_var  !< Integer variable to use for the lock
    integer(C_INT),   intent(in)         :: id        !< Identifier to give to this mutex

    if (.not. this % is_valid()) then
      this % c_location = C_LOC(lock_var)
      this % id = id
      call this % reset()
    end if
  end subroutine AMM_init_from_int

  !> Wait until this mutex is available and acquire it.
  subroutine AMM_lock(this)
    implicit none
    class(App_Mutex), intent(inout) :: this
    if (this % is_valid()) then
      call this % wait_timer % start()
      call acquire_idlock(this % c_location, this % id)
      call this % wait_timer % stop()
    end if
  end subroutine AMM_lock

  !> Release this mutex
  subroutine AMM_unlock(this)
    implicit none
    class(App_Mutex), intent(inout) :: this
    if (this % is_valid()) call release_idlock(this % c_location, this % id)
  end subroutine AMM_unlock

  !> Attempt to acquire this mutex. If it is already locked, return without acquiring it.
  !> \return Whether we successfully acquired this mutex. .false. if it is locked by someone else
  function AMM_try_lock(this) result(is_successfully_acquired)
    implicit none
    class(App_Mutex), intent(inout) :: this
    logical :: is_successfully_acquired

    is_successfully_acquired = .false.

    if (this % is_valid()) then
      if (try_acquire_idlock(this % c_location, this % id) .ne. 0) then
        is_successfully_acquired = .true.
      end if
    end if
  end function AMM_try_lock

  !> Check whether this mutex is already held (by anyone)
  function AMM_is_locked(this) result(is_locked)
    implicit none
    class(App_Mutex), intent(in) :: this
    logical :: is_locked

    integer(C_INT) :: is_locked_c

    is_locked = .false.
    if (this % is_valid()) then
      is_locked_c = is_lock_taken(this % c_location)
      if (is_locked_c .ne. 0) then
        is_locked = .true.
      end if
    end if
  end function AMM_is_locked

  !> Check whether this mutex is held this particular instance
  function AMM_is_locked_by_me(this) result(is_locked_by_me)
    implicit none
    class(App_Mutex), intent(in) :: this
    logical :: is_locked_by_me

    integer(C_INT) :: is_locked_c

    is_locked_by_me = .false.
    if (this % is_valid()) then
      is_locked_c = is_idlock_taken(this % c_location, this % id)
      if (is_locked_c .ne. 0) then
        is_locked_by_me = .true.
      end if
    end if
  end function AMM_is_locked_by_me

  !> Reset this mutex. If it was held by someone else, it will be considered as released
  subroutine AMM_reset(this)
    implicit none
    class(App_Mutex), intent(inout) :: this
    if (this % is_valid()) call reset_lock(this % c_location)
    if (this % wait_timer % is_valid()) call this % wait_timer % delete()
    call this % wait_timer % create()
  end subroutine AMM_reset

  !> Retrieve the identifier of this mutex
  function AMM_get_id(this) result(id)
    implicit none
    class(App_Mutex), intent(in) :: this
    integer(C_INT) :: id
    id = this % id
  end function AMM_get_id

  !> \return How long in total have we waited to acquire this mutex instance
  function AMM_get_total_wait_time_ms(this) result(wait_time)
    implicit none
    class(App_Mutex), intent(in) :: this
    real(C_DOUBLE) :: wait_time
    wait_time = this % wait_timer % get_total_time_ms()
  end function AMM_get_total_wait_time_ms

  !> \return How long we have waited to acquire this mutex instance in the latest lock call
  function AMM_get_latest_wait_time_ms(this) result(wait_time)
    implicit none
    class(App_Mutex), intent(in) :: this
    real(C_DOUBLE) :: wait_time
    wait_time = this % wait_timer % get_latest_time_ms()
  end function AMM_get_latest_wait_time_ms

end module App_Mutex_Module
