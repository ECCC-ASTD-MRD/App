#include "App_MPMD.h"
#include "App_build_info.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>

int Restart_Write(const char *Id,int Step) {

    FILE *file=NULL;

    if (!Id) return(TRUE);

    if (!(file=fopen(Id, "w"))) {
        App_Log(APP_ERROR, "\nUnable to write restart %s\n",Id);
        return(FALSE);
    } else {
       fprintf(file,"%d\n",Step);
       fclose(file);
    }

    App_Log(APP_VERBATIM, "\nWrote restart %s\n",Id);
    return(TRUE);
}

int Restart_Read(const char *Id) {

    struct stat attr;
    FILE *file=NULL;
    int   step=1;

    if (!Id) return(TRUE);

    if (stat(Id, &attr) == 0) {
       App_Log(APP_VERBATIM, "\nRead restart (%.24s)\n", ctime(&attr.st_mtime));
    } else {
       App_Log(APP_VERBATIM, "\nNo restart found\n",);
    }

    if (!(file=fopen(Id, "r"))) {
        App_Log(APP_ERROR, "\nUnable to read restart %s\n",Id);
        return(FALSE);
    } else {
       fscanf(file, "%d\n", &step);
       fclose(file);
    }

    return(step);
}

int32_t finalize() {

#ifdef HAVE_MPI
    MPI_Finalize();
#endif
    return(TRUE);
}

int main(int argc, char *argv[]) {

    int32_t step=0,delay=0,fail=-1,ok,trap=1;
    int64_t queued=0;
    char   *title=NULL;

#ifdef HAVE_MPI
    MPI_Init(NULL, NULL);
#endif

    TApp_Arg appargs[]=
      { { APP_INT32, &step,    1,             "s", "step",   "Number of step" },
        { APP_INT64, &queued,  1,             "q", "queued", "Queued time" },
        { APP_CHAR,  &title,   1,             "t", "title",  "Title run" },
        { APP_INT32, &delay,   1,             "d", "delay",  "timestep delay(s)" },
        { APP_INT32, &fail,    1,             "f", "fail",   "Force a PE to fail" },
        { APP_INT32, &trap,    1,             "a", "trap",   "Answer signals delay(s), 0 = no trap"},
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

    App->Step=Restart_Read(title);

    for(;(!step || App->Step<step);App->Step++) {
        App_Log(APP_INFO,"Step\n");
        if (trap && App_IsDone()) {
            // Trapped premption signal
            Restart_Write(title,App->Step);
            sleep(trap);
            break; 
        }
        if (delay)
           sleep(delay);

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