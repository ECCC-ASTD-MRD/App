#include "App.h"

int32_t finalize() {

    fprintf(stderr,"Finalizing\n");
    return(TRUE);
}

int main() {

    App->Finalize=finalize;

    App_Init(APP_MASTER, "finalize_c", "test", "finalize test", "now");
    App_Start();

    App_End(0);

    return(0);
}