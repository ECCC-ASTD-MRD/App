program test_app_fortran
    use app

    implicit none

    integer :: ier
    INTEGER, DIMENSION(-1:1, -1:2) :: A
    character(*), parameter :: fmt = "(a, a60)"

    call app_logstream('stdout')

    ! Call to app_loglevelno and app_log_level ensures initialization
    ier = app_loglevelno(APP_INFO)
    ier = app_toleranceno(APP_FATAL)

    write(app_msg, fmt) 'This is an INFO message after a call to app_loglevelno(APP_INFO)'  ! print message and return file type
    call app_log(APP_INFO, app_msg)

    write(app_msg, fmt) 'This is an error message'  ! print message and return file type
    call app_log(APP_ERROR, app_msg)
end
