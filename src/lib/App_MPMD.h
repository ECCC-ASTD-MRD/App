#ifndef MPMD_H__
#define MPMD_H__

#include <mpi.h>

#include "App.h"

int App_MPMD_Init();
void App_MPMD_Finalize();
MPI_Comm App_MPMD_GetSharedComm(const int32_t nbComponents, const int32_t components[nbComponents]);
MPI_Fint App_MPMD_GetSharedComm_F(const int32_t nbComponents, const int32_t components[nbComponents]);
MPI_Comm App_MPMD_GetSelfComm();
MPI_Fint App_MPMD_GetSelfComm_F();
int32_t App_MPMD_HasComponent(const char * const componentName);
int App_MPMD_GetComponentId(const char * const componentName);
int App_MPMD_GetSelfComponentId();
const char * App_MPMD_ComponentIdToName(const int componentId);
int App_MPMD_GetSelfComponentRank();
int App_MPMD_GetSelfComponentSize();
int App_MPMD_GetComponentSize(const int componentId);
int App_MPMD_GetComponentPeWRank(const int componentId, const int localRank);

#endif // MPMD_H__
