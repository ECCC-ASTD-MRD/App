#ifndef MPMD_H__
#define MPMD_H__

//! \file
//! MPMD interface definition

#include <mpi.h>

#include "App.h"

TApp * App_MPMD_Init(const char * const ComponentName, const char * const version);
void App_MPMD_Finalize();
MPI_Comm App_MPMD_GetSharedComm(const int32_t * const Components, const int32_t NumComponents);
MPI_Fint App_MPMD_GetSharedComm_F(const int32_t * const Components, const int32_t NumComponents);
MPI_Comm App_MPMD_GetSelfComm();
MPI_Fint App_MPMD_GetSelfComm_F();
int32_t App_MPMD_HasComponent(const char * const ComponentName);
int App_MPMD_GetComponentId(const char * const componentName);
int App_MPMD_GetSelfComponentId();
const char * App_MPMD_ComponentIdToName(const int ComponentId);

#endif // MPMD_H__
