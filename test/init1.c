#include "App_MPMD.h"


int main() {
    MPI_Init(NULL, NULL);
    App_Init(APP_MASTER, "init1", "test", "mpmd context attempt", "now");
    int componentId = -1;
    App_MPMD_Init(&componentId);
    App_Start();

    App_End(0);
    App_MPMD_Finalize();
    MPI_Finalize();
    return 0;
}