
interface
    function shmem_allocate_shared(shmid, size) result(ptr) BIND(C,name='shmem_allocate_shared')
        import :: C_PTR, C_INT, C_INT64_T
        implicit none
        integer(C_INT), intent(OUT) :: shmid           !< shared memory id of segment (set by shmem_allocate_shared) (see shmget)
        integer(C_INT64_T), intent(IN), value :: size  !< size of segment in bytes
        type(C_PTR) :: ptr                             !< local address of memory segment
    end function shmem_allocate_shared
    function shmem_address_from_id(shmid) result(ptr) BIND(C,name='shmem_address_from_id')
        import :: C_PTR, C_INT
        implicit none
        integer(C_INT), intent(IN), value :: shmid     !< shared memory id of segment (see shmget)
        type(C_PTR) :: ptr                             !< local address of shared memory segment
    end function shmem_address_from_id
end interface
