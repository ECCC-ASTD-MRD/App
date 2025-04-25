#include "App_MPMD.h"


int main() { 
    MPI_Init(NULL, NULL);

    App_Init(APP_MASTER, "init2", "test", "mpmd context attempt", "now");
    App_Start();

    App_MPMD_Init();
    App_MPMD_Finalize();

    App_End(0);

    MPI_Finalize();
}