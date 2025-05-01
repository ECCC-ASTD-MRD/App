
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


int main() {
    MPI_Init(NULL, NULL);

    App_Init(APP_MASTER, "mpmd_5", "test", "mpmd context attempt", "now");
    App_MPMD_Init();
    App_Start();

    const int mpmd_5id = App_MPMD_GetSelfComponentId();

    validate_comm_size(App_MPMD_GetSelfComm(), 5);

    const int mpmd_1id = App_MPMD_GetComponentId("mpmd_1");

    if (!App_MPMD_HasComponent("mpmd_1")) {
        App_Log(APP_ERROR, "%s: Can only be launched if MPMD_1 is also present\n", __func__);
        exit(1);
    }

    const MPI_Comm comm_15 = App_MPMD_GetSharedComm(2, (int[]){mpmd_1id, mpmd_5id});
    validate_comm_size(comm_15, 1 + 5);

    App_End(0);
    App_MPMD_Finalize();

    MPI_Finalize();

    return(0);
}
