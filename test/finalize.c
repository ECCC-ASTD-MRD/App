#include "App.h"

int32_t finalize() {

    fprintf(stderr,"Finalizing\n");
    return(TRUE);
}

int main() {

    App_FinalizeCallback(finalize);
//    App->Finalize=finalize;

    App_Init(APP_MASTER, "finalize_c", "test", "finalize test", "now");
    App_Start();

    App_Stats(NULL);

    App_End(0);

    return(0);
}