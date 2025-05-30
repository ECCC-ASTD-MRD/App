#include "App_Shared_Memory.h"

#include <sys/ipc.h>
#include <sys/shm.h>

//! Allocate a shared memory segment
//! \return Local address of memory block
void* shmem_allocate_shared(
    int* const shmid,           //!< [out] shared memory id of segment (set by shmem_allocate_shared) (see shmget)
    const size_t size           //!< [in]  size of segment in bytes
) {
    const int id = shmget(IPC_PRIVATE, size, 0600); // get a memory block, only accessible by user
    *shmid = id;                                    // shared memory block id returned to caller
    if (id == -1) return NULL;

    void* shmaddr = shmat(id, NULL, 0);             // get local address of shared memory block
    if (shmaddr == NULL) return NULL;

    struct shmid_ds dummy;
    int err = shmctl(id, IPC_RMID, &dummy);         // mark block "to be deleted when no process attached"
    if (err == -1) {
        // Cleanup after failure
        err = shmdt(shmaddr);
        return NULL;
    }

    return shmaddr;     // return local address of memory block
}

//! Get memory address associated with shared memory segment id
//! \return Local memory addres of shared memory segment
void *shmem_address_from_id(
    const int shmid     //!< [in] shared memory id of segment (see shmget)
) {
    void* p = shmat(shmid, NULL, 0) ;
    return p;
}