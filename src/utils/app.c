#include "App_MPMD.h"
#include "App_build_info.h"

int32_t finalize() {

#ifdef HAVE_MPI
    MPI_Finalize();
#endif
    return(TRUE);
}

int main(int argc, char *argv[]) {

    int32_t step=0,fail=-1,ok;
    int64_t queued=0;
    char   *title=NULL;

#ifdef HAVE_MPI
    MPI_Init(NULL, NULL);
#endif

    TApp_Arg appargs[]=
      { { APP_INT32, &step,    1,             "s", "step",   "Number of step" },
        { APP_INT64, &queued,  1,             "q", "queued", "Queued time" },
        { APP_CHAR,  &title,   1,             "t", "title",  "Title run" },
        { APP_INT32, &fail,    1,             "f", "fail",   "Force a PE to fail" },
        { APP_NIL } };

    if (!App_ParseArgs(appargs,argc,argv,APP_ARGSLOG)) {
       exit(EXIT_FAILURE);
    }

    App_Init(APP_MASTER,title?title:"app",VERSION,PROJECT_DESCRIPTION_STRING,GIT_COMMIT_TIMESTAMP);
    App_FinalizeCallback(finalize);
    App_Start();

    // In fail mode test, we need to enable the tolerance level
    if (fail>=0) {
        App_ToleranceLevel("FATAL");
    }
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

        // Make a rank fail
        if (fail>=0) {
            App_LogAllRanks((App->RankMPI==fail?APP_FATAL:APP_QUIET)+APP_COLLECT,"Fail in rank %i\n",fail);
        }
    }

    ok=App_End(0);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return(ok);
}