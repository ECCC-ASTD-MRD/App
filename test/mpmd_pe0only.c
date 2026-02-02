#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "App_MPMD.h"

void validate_comm_size(const MPI_Comm comm, const int expected_num_procs) {
    if (expected_num_procs == 0) {
        if (comm != MPI_COMM_NULL) {
            App_Log(APP_ERROR, "We were expecting a NULL communicator!\n");
            exit(1);
        }
    }

    int num_procs;
    MPI_Comm_size(comm, &num_procs);

    if (num_procs != expected_num_procs) {
        App_Log(APP_ERROR, "We have %d PEs, but we should have %d!\n", num_procs, expected_num_procs);
        exit(1);
    }
}


int main(int argc, char * argv[]) {
    MPI_Init(NULL, NULL);

    if (argc != 2) {
        printf("Usage: mpirun -n 3 ./mpmd_pe0only 3 : -n 5 ./mpmd_pe0only 5 : -n 7 ./mpmd_pe0only 7\n");
        exit(3);
    }

    const int componentNameMaxLen = 9;
    char componentName[componentNameMaxLen];
    const int nbPe = atoi(argv[1]);
    snprintf(componentName, componentNameMaxLen, "mpmd_%03d", nbPe);
    App_Init(APP_MASTER, componentName, "test", "mpmd context attempt", "now");
    App_MPMD_Init();
    App_Start();

    const int componentId = App_MPMD_GetSelfComponentId();
    const int size = App_MPMD_GetSelfComponentSize();
    if (size != nbPe) {
        printf("Component size (%d) does not match expected size (%d)!\n", size, nbPe);
        exit(2);
    }
    const int componentRank = App_MPMD_GetSelfComponentRank();
    const int mpmdWorldRank = App_MPMD_GetComponentPeWRank(componentId, componentRank);
    int worldRank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    if (worldRank != mpmdWorldRank) {
        printf("worldRank = %03d, mpmdWorldRank = %03d\n", worldRank, mpmdWorldRank);
        exit(3);
    }

    const int mpmd_3id = App_MPMD_GetComponentId("mpmd_003");
    const int mpmd_5id = App_MPMD_GetComponentId("mpmd_005");
    const int mpmd_7id = App_MPMD_GetComponentId("mpmd_007");

    MPI_Barrier(MPI_COMM_WORLD);

    const MPI_Comm comm_pe0 = App_MPMD_GetSharedComm(3, (int[]){mpmd_3id, mpmd_5id, mpmd_7id}, 1);
    // Only the PE0 of each component will add a non-null communicator
    if (componentRank == 0) {
        validate_comm_size(comm_pe0, 3);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    const MPI_Comm comm_all = App_MPMD_GetSharedComm(3, (int[]){mpmd_3id, mpmd_5id, mpmd_7id}, 0);
    int sum = 0;
    if (componentRank == 0) MPI_Allreduce(&nbPe, &sum, 1, MPI_INT, MPI_SUM, comm_pe0);
    MPI_Bcast(&sum, 1, MPI_INT, 0, MPI_COMM_WORLD);
    validate_comm_size(comm_all, sum);

    App_End(0);
    App_MPMD_Finalize();

    MPI_Finalize();
    return 0;
}
