#ifndef APP_SHARED_MEMORY_H__
#define APP_SHARED_MEMORY_H__

#include <stdlib.h>

void* shmem_allocate_shared(int* const shmid, const size_t size);
void* shmem_address_from_id(const int shmid);

#endif
