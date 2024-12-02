
interface
    subroutine acquire_idlock(lock, id) BIND(C, name = 'acquire_idlock_F')
        import :: C_INT, C_PTR
        implicit none
        type(C_PTR),    intent(in), value :: lock
        integer(C_INT), intent(in), value :: id
    end subroutine acquire_idlock
    subroutine release_idlock(lock, id) BIND(C, name = 'release_idlock_F')
        import :: C_INT, C_PTR
        implicit none
        type(C_PTR),    intent(in), value :: lock
        integer(C_INT), intent(in), value :: id
    end subroutine release_idlock
    function try_acquire_idlock(lock, id) result(is_successfully_acquired) BIND(C, name = 'try_acquire_idlock_F')
        import :: C_INT, C_PTR
        implicit none
        type(C_PTR),    intent(in), value :: lock
        integer(C_INT), intent(in), value :: id
        integer(C_INT) :: is_successfully_acquired
    end function try_acquire_idlock
    function is_idlock_taken(lock, id) result(is_locked) BIND(C, name = 'is_idlock_taken_F')
        import :: C_INT, C_PTR
        implicit none
        type(C_PTR),    intent(in), value :: lock
        integer(C_INT), intent(in), value :: id
        integer(C_INT) :: is_locked
    end function is_idlock_taken
    function is_lock_taken(lock) result(is_locked) BIND(C, name = 'is_lock_taken_F')
        import :: C_INT, C_PTR
        implicit none
        type(C_PTR),    intent(in), value :: lock
        integer(C_INT) :: is_locked
    end function is_lock_taken
    subroutine reset_lock(lock) BIND(C, name = 'reset_lock_F')
        import :: C_PTR
        implicit none
        type(C_PTR), intent(in), value :: lock
    end subroutine reset_lock

    function try_update_int32(variable_ptr, old_value, new_value) result(status) BIND(C, name = 'try_update_int32_F')
        import :: C_PTR, C_INT32_T
        implicit none
        type(C_PTR), intent(in), value :: variable_ptr
        integer(C_INT32_T), intent(in), value :: old_value, new_value
        integer(C_INT32_T) :: status
    end function try_update_int32

    function atomic_add_int32(var_ptr, increment) BIND(C, name = 'atomic_add_int32_F') result(new_value)
        import C_PTR, C_INT32_T
        type(C_PTR),        intent(in), value :: var_ptr
        integer(C_INT32_T), intent(in), value :: increment
        integer(C_INT32_T) :: new_value
    end function atomic_add_int32

end interface
