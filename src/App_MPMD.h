#ifndef MPMD_H__
#define MPMD_H__

#include <mpi.h>

#include "App.h"

TApp*       App_MPMD_Init(const char * const ComponentName, const char * const version);
void        App_MPMD_Finalize();
MPI_Comm    App_MPMD_GetSharedComm(const int32_t * Components, const int32_t NumComponents);
MPI_Fint    App_MPMD_GetSharedComm_F(const int32_t * Components, const int32_t NumComponents);
MPI_Comm    App_MPMD_GetOwnComm(void);
MPI_Fint    App_MPMD_GetOwnComm_F(void);
int32_t     App_MPMD_HasComponent(const char * const ComponentName);
const char* App_MPMD_ComponentIdToName(const int ComponentId);

#endif // MPMD_H__
