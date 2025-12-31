program finalize
    use app
    use, intrinsic :: iso_c_binding
    implicit none

    type(C_FUNPTR) :: func_ptr_to_c
    integer :: n

    func_ptr_to_c=C_FUNLOC(finalizef)
    call app_finalizecallback(func_ptr_to_c)
    app_ptr = App_Init(0, "finalize_f", "test", "finalize test", "now")
    call App_Start()
   
    call app_stats('')

    app_status=app_end(0)

contains

    function finalizef()
        use, intrinsic :: iso_c_binding
        implicit none

        integer(C_INT32_T) :: finalizef

        write(6,*) "Finalizing"
        finalizef=1

        return
    end function finalizef

end program finalize
