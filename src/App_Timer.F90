
module App_Timer_Module
    use iso_c_binding
    implicit none
    private

    type, public :: App_Timer
      type(C_PTR) :: c_timer = C_NULL_PTR !< Pointer to the C struct containing the timer
    contains
      procedure :: is_valid => timer_is_valid
      procedure :: create   => timer_create
      procedure :: delete   => timer_delete
      procedure :: start    => timer_start
      procedure :: stop     => timer_stop
      procedure :: get_total_time_ms       => timer_get_total_time_ms
      procedure :: get_latest_time_ms      => timer_get_latest_time_ms
      procedure :: get_time_since_start_ms => timer_get_time_since_start_ms
    end type App_Timer

    interface
        subroutine App_TimerInit(timer) bind(C, name = 'App_TimerInit_f')
            import :: C_PTR
            implicit none
            type(C_PTR), intent(in), value :: timer
        end subroutine
        function App_TimerCreate() result(timer) BIND(C, name = 'App_TimerCreate_f')
            import C_PTR
            implicit none
            type(C_PTR) :: timer
        end function
        subroutine App_TimerDelete(timer) BIND(C, name = 'App_TimerDelete_f')
            import C_PTR
            implicit none
            type(C_PTR), intent(IN), value :: timer
        end subroutine
        subroutine App_TimerStart(timer) BIND(C, name = 'App_TimerStart_f')
            import C_PTR
            implicit none
            type(C_PTR), intent(IN), value :: timer
        end subroutine
        subroutine App_TimerStop(timer) BIND(C, name = 'App_TimerStop_f')
            import C_PTR
            implicit none
            type(C_PTR), intent(IN), value :: timer
        end subroutine
        function App_TimerTotalTime_ms(timer) result(time) BIND(C, name = 'App_TimerTotalTime_ms_f')
            import C_PTR, C_DOUBLE
            implicit none
            type(C_PTR), intent(IN), value :: timer
            real(C_DOUBLE) :: time
        end function
        function App_TimerLatestTime_ms(timer) result(time) BIND(C, name = 'App_TimerLatestTime_ms_f')
            import C_PTR, C_DOUBLE
            implicit none
            type(C_PTR), intent(IN), value :: timer
            real(C_DOUBLE) :: time
        end function
        function App_TimerTimeSinceStart_ms(timer) result(time) BIND(C, name = 'App_TimerTimeSinceStart_ms_f')
            import C_PTR, C_DOUBLE
            implicit none
            type(C_PTR), intent(IN), value :: timer
            real(C_DOUBLE) :: time
        end function
    end interface
contains

  function timer_is_valid(this) result(is_valid)
    implicit none
    class(App_Timer), intent(in) :: this
    logical :: is_valid
    is_valid = c_associated(this % c_timer)
  end function timer_is_valid

  !> \copyparam IO_timer_create
  subroutine timer_create(this)
    implicit none
    class(App_Timer), intent(inout) :: this
    if (.not. c_associated(this % c_timer)) then
      this % c_timer = App_TimerCreate()
    else
      call App_TimerInit(this % c_timer)
    end if
  end subroutine timer_create

  !> \copyparam IO_timer_delete
  subroutine timer_delete(this)
    implicit none
    class(App_Timer), intent(inout) :: this
    call App_TimerDelete(this % c_timer)
    this % c_timer = C_NULL_PTR
  end subroutine timer_delete

  !> \copyparam IO_timer_start
  subroutine timer_start(this)
    implicit none
    class(App_Timer), intent(inout) :: this
    call App_TimerStart(this % c_timer)
  end subroutine timer_start

  !> \copyparam IO_timer_stop
  subroutine timer_stop(this)
    implicit none
    class(App_Timer), intent(inout) :: this
    call App_TimerStop(this % c_timer)
  end subroutine timer_stop

  !> \copyparam IO_total_time_ms
  function timer_get_total_time_ms(this) result(time)
    implicit none
    class(App_Timer), intent(in) :: this
    real(C_DOUBLE) :: time
    time = App_TimerTotalTime_ms(this % c_timer)
  end function timer_get_total_time_ms

  !> \copyparam IO_latest_time_ms
  function timer_get_latest_time_ms(this) result(time)
    implicit none
    class(App_Timer), intent(in) :: this
    real(C_DOUBLE) :: time
    time = App_TimerLatestTime_ms(this % c_timer)
  end function timer_get_latest_time_ms

  !> \copyparam IO_time_since_start_ms
  function timer_get_time_since_start_ms(this) result(time)
    implicit none
    class(App_Timer), intent(in) :: this
    real(C_DOUBLE) :: time
    time = App_TimerTimeSinceStart_ms(this % c_timer)
  end function timer_get_time_since_start_ms

end module App_Timer_Module