
module App_Atomic_Module

    use iso_c_binding
    implicit none
    include 'App_Atomic.inc'

    !> 32-bit integer type that allows atomic operations
    !> Can be used across processes, if they use a location in shared memory
    type, public :: atomic_int32
        private
        type(C_PTR) :: c_location = C_NULL_PTR
    contains
        procedure, pass :: init_from_int  => atomic_int32_init_from_int
        procedure, pass :: add            => atomic_int32_add
        procedure, pass :: try_update     => atomic_int32_try_update
        procedure, pass :: read           => atomic_int32_read
    end type atomic_int32

contains

    !> Initialize atomic_int32 type to use the specified variable
    subroutine atomic_int32_init_from_int(this, variable)
        implicit none
        class(atomic_int32), intent(inout) :: this
        integer(C_INT32_T),  intent(in), target :: variable !< Variable (location) that will be used for atomic operations
        this % c_location = c_loc(variable)
    end subroutine atomic_int32_init_from_int

    !> Atomically add specified amount to this variable
    function atomic_int32_add(this, increment) result(new_value)
        implicit none
        class(atomic_int32), intent(inout) :: this
        integer(C_INT32_T),  intent(in)    :: increment !< How much we want to add
        integer(C_INT32_T) :: new_value
        new_value = atomic_add_int32(this % c_location, increment)
    end function atomic_int32_add

    !> Attempt to atomically add specified amount to this variable
    !> \return Whether the attempt was successful (.false. if someone else updated at the same time)
    function atomic_int32_try_update(this, old_value, new_value) result(has_updated)
        implicit none
        class(atomic_int32), intent(inout) :: this !< atomic_int32 instance
        integer(C_INT32_T),  intent(in)    :: old_value
        integer(C_INT32_T),  intent(in)    :: new_value
        logical :: has_updated
        has_updated = .false.
        if (c_associated(this % c_location)) has_updated = (try_update_int32(this % c_location, old_value, new_value) == 1)
    end function

    !> Atomically read the value of this variable
    function atomic_int32_read(this) result(val)
        implicit none
        class(atomic_int32), intent(in) :: this
        integer(C_INT32_T) :: val
        integer, pointer :: value_ptr
        call c_f_pointer(this % c_location, value_ptr)
        val = value_ptr
    end function atomic_int32_read


end module App_Atomic_Module
