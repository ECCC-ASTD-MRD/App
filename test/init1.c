#include "App_MPMD.h"


int main(int argc, char* argv[]) {
    App_MPMD_Init("Init1", "0.0.0", MPI_THREAD_SINGLE);
    const int test5id = App_MPMD_GetSelfComponentId();

    App_MPMD_Finalize();
    return 0;
}