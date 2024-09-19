#ifndef MPMD_H__
#define MPMD_H__

#include <mpi.h>

#include "App.h"

typedef enum {
    APP_MPMD_NONE        = 0,
    APP_MPMD_GEM_ID      = 1,
    APP_MPMD_IOSERVER_ID = 2,
    APP_MPMD_IRIS_ID     = 3,
    APP_MPMD_NEMO_ID     = 4,

    APP_MPMD_TEST1_ID = 1001,
    APP_MPMD_TEST2_ID = 1002,
    APP_MPMD_TEST3_ID = 1003,
    APP_MPMD_TEST4_ID = 1004,
    APP_MPMD_TEST5_ID = 1005,
} TApp_MPMDId;

TApp*       App_MPMD_Init(const TApp_MPMDId ComponentId);
void        App_MPMD_Finalize(/*TApp* app*/);
MPI_Comm    App_MPMD_GetSharedComm(const int32_t* Components, const int32_t NumComponents);
MPI_Fint    App_MPMD_GetSharedComm_F(const int32_t* Components, const int32_t NumComponents);
MPI_Comm    App_MPMD_GetOwnComm(void);
MPI_Fint    App_MPMD_GetOwnComm_F(void);
int32_t     App_MPMD_HasComponent(const TApp_MPMDId ComponentId);
const char* App_MPMD_ComponentIdToName(const TApp_MPMDId ComponentId);

#endif // MPMD_H__
