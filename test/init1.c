#include "App_MPMD.h"


int main() {
    App_MPMD_Init("Init1", "0.0.0", MPI_THREAD_SINGLE);
    App_MPMD_Finalize();
    return 0;
}