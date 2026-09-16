program finalize
    use app
    use, intrinsic :: iso_c_binding, only : C_FUNPTR
    implicit none

    type(C_FUNPTR) :: func_ptr_to_c

    func_ptr_to_c = C_FUNLOC(finalizef)
    call app_finalizecallback(func_ptr_to_c)
    app_ptr = App_Init(0, "finalize_f", "test", "finalize test", "now")
    call App_Start()

    call app_logstats('FORTRAN')

    app_status = app_end(0)

contains

    function finalizef()
        use, intrinsic :: iso_c_binding, only : C_INT32_T
        implicit none

        integer(C_INT32_T) :: finalizef

        call app_log(APP_INFO,"Finalizing");
        finalizef = 1
    end function finalizef

end program finalize
