#include "App_MPMD.h"


int main() {
    MPI_Init(NULL, NULL);
    App_Init(APP_MASTER, "init1", "test", "mpmd context attempt", "now");
    const int componentId = App_MPMD_Init();
    if (componentId < 0) return 1;
    App_Start();

    App_End(0);
    App_MPMD_Finalize();
    MPI_Finalize();
    return 0;
}