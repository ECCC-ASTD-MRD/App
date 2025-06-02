#include "App_MPMD.h"
#include "App_build_info.h"


int main(int argc, char *argv[]) {

    int32_t step=10,ok;
    int64_t queued=0;
    char   *title=NULL;

    MPI_Init(NULL, NULL);

    TApp_Arg appargs[]=
      { { APP_INT32, &step,    1,             "s", "step",   "Number of step" },
        { APP_INT64, &queued,  1,             "q", "queued", "Queued time" },
        { APP_CHAR,  &title,   1,             "t", "title",  "Title run" },
        { APP_NIL } };

    if (!App_ParseArgs(appargs,argc,argv,APP_ARGSLOG)) {
       exit(EXIT_FAILURE);
    }

    App_Init(APP_MASTER,title?title:"app",VERSION,PROJECT_DESCRIPTION_STRING,GIT_COMMIT_TIMESTAMP);
    App_Start();

    if (queued) {
       App_Log(APP_VERBATIM, "\nWaiting time   : %li s\n",App->Time.tv_sec-queued);
    }
    App_NodePrint();

    for(App->Step=1;(!step || App->Step<step);App->Step++) {
        App_Log(APP_INFO,"Step\n");
        if (App_IsDone()) {
           // Trapped premption signal
           break; 
        }
    }

    ok=App_End(0);

    MPI_Finalize();

    return(ok);
}