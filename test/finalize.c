#include "App.h"

int32_t finalize() {

    App_Log(APP_INFO,"Finalizing\n");
    return(TRUE);
}

int main() {

    App_FinalizeCallback(finalize);

    App_Init(APP_MASTER, "finalize_c", "test", "finalize test", "now");
    App_Start();

    App_LogStats("C");

    App_End(0);

    return(0);
}