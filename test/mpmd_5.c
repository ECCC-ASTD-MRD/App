
#include "App_MPMD.h"

void validate_comm_size(MPI_Comm comm, const int expected_num_procs) {
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

int main(int argc, char* argv[])
{
    App_MPMD_Init(APP_MPMD_TEST5_ID);

    validate_comm_size(App_MPMD_GetOwnComm(), 5);

    if (!App_MPMD_HasComponent(APP_MPMD_TEST1_ID)) {
        App_Log(APP_ERROR, "%s: Can only be launched if MPMD_1 is also present\n", __func__);
        exit(1);
    }

    const MPI_Comm comm_15 = App_MPMD_GetSharedComm((int[]){APP_MPMD_TEST5_ID, APP_MPMD_TEST1_ID}, 2);
    validate_comm_size(comm_15, 1 + 5);

    App_MPMD_Finalize();
    return 0;
}
