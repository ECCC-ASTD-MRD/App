#include "App_MPMD.h"
#include "App_Timer.h"
#include "App_build_info.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>

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
        App_Log(APP_VERBATIM, "Found restart (%.24s)\n", ctime(&attr.st_mtime));
        if (!(file=fopen(Id, "r"))) {
            App_Log(APP_ERROR, "\nUnable to read restart %s\n",Id);
            return(FALSE);
        } else {
            fscanf(file, "%d\n", &step);
            fclose(file);
        }
    } else {
       App_Log(APP_VERBATIM, "\nNo restart found\n");
    }


    return(step);
}

int Buffer_Write(char* SubDir,uint32_t MBytes) {

    int fd=-1;
    char path[1024],dir[1024];
    int  rank;
    uint32_t nc=0, buffer[1024*1024] ;
    TApp_Timer *timer=App_TimerCreate();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank) ;

    if (SubDir) {
       snprintf(dir, sizeof(dir), "%s%06d",SubDir,rank);
       App_Log(APP_INFO, "\nWriting path %s\n",dir);
       mkdir(dir, 0777);
       snprintf(path, sizeof(path), "%s/out%06d", dir,rank);
    } else {
       snprintf(path, sizeof(path), "out%06d", rank);
    }

    if ((fd=open(path, O_RDWR | O_CREAT, 0777))) {
        App_TimerStart(timer);
        for(uint32_t i=0 ; i<MBytes ; i++){
            nc += write(fd, buffer, sizeof(buffer)) ;
        }
        close(fd) ;
        App_TimerStop(timer);
    }
    App_LogAllRanks(APP_INFO, "\nWrote %i MB to %s in %s\n",MBytes,path,App_TimeString(timer,APP_LATEST));

    return(TRUE);
}

int32_t Finalize() {

#ifdef HAVE_MPI
    MPI_Finalize();
#endif
    return(TRUE);
}

int main(int argc, char *argv[]) {

    int32_t step=0,delay=0,fail=-1,ok,trap=1,out=0;
    int64_t queued=0;
    char   *title=NULL,*outdir=NULL;

#ifdef HAVE_MPI
    MPI_Init(NULL, NULL);
#endif

    TApp_Arg appargs[]=
      { { APP_INT32, &step,    1,             "s", "step",   "Number of step" },
        { APP_INT64, &queued,  1,             "q", "queued", "Queued time" },
        { APP_CHAR,  &title,   1,             "t", "title",  "Title run" },
        { APP_INT32, &delay,   1,             "d", "delay",  "timestep delay(s)" },
        { APP_INT32, &fail,    1,             "f", "fail",   "Force a PE to fail" },
        { APP_INT32, &out,     1,             "o", "output", "Output size (default:0 = no output)" },
        { APP_CHAR,  &outdir,  1,             "p", "path",   "Output path (default:'' = no directory)" },
        { APP_INT32, &trap,    1,             "a", "trap",   "Answer signals delay(s) (default:0 = no trap)"},
        { APP_NIL } };

    if (!App_ParseArgs(appargs,argc,argv,APP_ARGSLOG)) {
       exit(EXIT_FAILURE);
    }

    App_Init(APP_MASTER,title?title:"app",VERSION,PROJECT_DESCRIPTION_STRING,GIT_COMMIT_TIMESTAMP);
    App_FinalizeCallback(Finalize);
    App_Start();

    App_Trap(SIGUSR1);
    App_Trap(SIGUSR2);

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

        if (out){
           Buffer_Write(outdir,out);
        }

        if (delay)
           sleep(delay);

        // Make a rank fail
        if (fail>=0) {
            App_LogAllRanks((App->RankMPI==fail?APP_FATAL:APP_QUIET)+APP_COLLECT,"Fail in rank %i\n",fail);
        }
        App_LogStats("");
    }

    ok=App_End(0);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return(ok);
}