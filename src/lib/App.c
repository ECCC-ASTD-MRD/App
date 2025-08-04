//! \file
//! Implementation of the App library

#define APP_BUILD

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <signal.h>
#include <sys/utsname.h>
#ifndef _AIX
   #include <sys/syscall.h>
#endif

#include <pthread.h>

#include "App.h"
#include "App_build_info.h"
#include "str.h"

static TApp AppInstance;                             ///< Static App instance
__thread TApp *App = &AppInstance;                   ///< Per thread App pointer
__thread char App_Buf[32];                           ///< Per thread char buffer
static __thread char APP_LASTERROR[APP_ERRORSIZE];   ///< Last error is accessible through this

static pthread_mutex_t App_mutex = PTHREAD_MUTEX_INITIALIZER;

static char* AppMemUnits[]    = { "KB", "MB", "GB", "TB" };
static char* AppLibNames[]    = { "main", "rmn", "fst", "brp", "wb", "gmm", "vgrid", "interpv", "georef", "rpnmpi", "iris", "io", "mdlutil", "dyn", "phy", "midas", "eer", "tdpack", "mach", "spsdyn", "meta" };
static char* AppLibLog[]      = { "", "RMN|", "FST|", "BRP|", "WB|", "GMM|", "VGRID|", "INTERPV|", "GEOREF|", "RPNMPI|", "IRIS|", "IO|", "MDLUTIL|", "DYN|", "PHY|", "MIDAS|", "EER|", "TDPACK|", "MACH|", "SPSDYN|", "META|" };
static char* AppLevelNames[]  = { "INFO", "FATAL", "SYSTEM", "ERROR", "WARNING", "INFO", "STAT", "TRIVIAL", "DEBUG", "EXTRA" };
static char* AppLevelColors[] = { "", APP_COLOR_RED, APP_COLOR_RED, APP_COLOR_RED, APP_COLOR_YELLOW, "", APP_COLOR_BLUE, "", APP_COLOR_LIGHTCYAN, APP_COLOR_CYAN };

unsigned int App_OnceTable[APP_MAXONCE];         ///< Log once table

//! Return last error
char* App_ErrorGet(void)     { return APP_LASTERROR; }

TApp* App_GetInstance(void)  { return &AppInstance; }
int   App_IsDone(void)       { return App->State == APP_DONE; }
int   App_IsMPI(void)        { return App->NbMPI > 1; }
int   App_IsOMP(void)        { return App->NbThread > 1; }
int   App_IsSingleNode(void) { return App->NbNodeMPI == App->NbMPI; }
int   App_IsAloneNode(void)  { return App->NbNodeMPI == 1; }


#ifdef HAVE_MPI
int App_MPIProcCmp(const void *a, const void *b) {
    return strncmp((const char*)a, (const char*)b, MPI_MAX_PROCESSOR_NAME);
}


void App_SetMPIComm(MPI_Comm Comm) {
    App->Comm = Comm;

    // Initialize MPI.
    int mpiIsInit;
    MPI_Initialized(&mpiIsInit);
    if (mpiIsInit) {
        MPI_Comm_size(App->Comm, &App->NbMPI);
        MPI_Comm_rank(App->Comm, &App->RankMPI);

        App->TotalsMPI = (int*)realloc(App->TotalsMPI, (App->NbMPI + 1) * sizeof(int));
        App->CountsMPI = (int*)realloc(App->CountsMPI, (App->NbMPI + 1) * sizeof(int));
        App->DisplsMPI = (int*)realloc(App->DisplsMPI, (App->NbMPI + 1) * sizeof(int));
    }
}
#endif

//! Ajouter une librairie (info pour header de log)
void App_LibRegister(
    //! [in] Library
    const TApp_Lib Lib,
    //! [in] Version
    const char * const Version
) {
    APP_FREE(App->LibsVersion[Lib]);
    if (Version) App->LibsVersion[Lib] = strdup(Version);
}


//! Initialiser l'environnement dans la structure App
void App_InitEnv() {
    pthread_mutex_lock(&App_mutex);
    {
        // Only do it if not already initialized
        if (!App->Tolerance) {
            gettimeofday(&App->Time, NULL);

            App->TimerLog = App_TimerCreate();
            App->Tolerance = APP_QUIET;
            App->Language = APP_EN;
            App->LogWarning = 0;
            App->LogError = 0;
            App->LogColor = FALSE;
            App->LogNoBox = FALSE;
            App->LogTime = FALSE;
            App->LogSplit = FALSE;
            App->LogFlush = FALSE;
            App->LogRank = 0;
            App->UTC = FALSE;

            // Default log level is WARNING
            for(int l = 0; l < APP_LIBSMAX; l++) App->LogLevel[l] = APP_WARNING;

            char * envVarVal;

            // Check the log parameters in the environment
            if ((envVarVal = getenv("APP_VERBOSE"))) {
                // \bug This can cause a deadlock or at least lock timeout since the App_LogLevel function calls Lib_LogLevel which in turn
                // calls App_InitEnv and there is a mutex around a entire code of this function
                App_LogLevel(envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_NOBOX"))) {
                App->LogNoBox = TRUE;
            }
            if ((envVarVal = getenv("APP_VERBOSE_COLOR"))) {
                App->LogColor = TRUE;
            }
            if ((envVarVal = getenv("APP_VERBOSE_TIME"))) {
                App_LogTime(envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_UTC"))) {
                App->UTC = TRUE;
            }
            if ((envVarVal = getenv("APP_VERBOSE_RANK"))) {
                App->LogRank = atoi(envVarVal);
            }
            if ((envVarVal = getenv("APP_LOG_SPLIT"))) {
                App->LogSplit = TRUE;
            }
            if ((envVarVal = getenv("APP_LOG_STREAM"))) {
                App->LogFile = strdup(envVarVal);
            }
            if ((envVarVal = getenv("APP_LOG_FLUSH"))) {
                App->LogFlush = TRUE;
            }
            if ((envVarVal = getenv("APP_TOLERANCE"))) {
                App_ToleranceLevel(envVarVal);
            }
            if ((envVarVal = getenv("APP_NOTRAP"))) {
                App->Signal = -1;
            }

            // Check verbose level of libraries
            if ((envVarVal = getenv("APP_VERBOSE_RMN"))) {
                Lib_LogLevel(APP_LIBRMN, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_FST"))) {
                Lib_LogLevel(APP_LIBFST, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_BRP"))) {
                Lib_LogLevel(APP_LIBBRP, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_WB"))) {
                Lib_LogLevel(APP_LIBWB, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_GMM"))) {
                Lib_LogLevel(APP_LIBGMM, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_VGRID"))) {
                Lib_LogLevel(APP_LIBVGRID, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_INTERPV"))) {
                Lib_LogLevel(APP_LIBINTERPV, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_GEOREF"))) {
                Lib_LogLevel(APP_LIBGEOREF, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_RPNMPI"))) {
                Lib_LogLevel(APP_LIBRPNMPI, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_IRIS"))) {
                Lib_LogLevel(APP_LIBIRIS, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_IO"))) {
                Lib_LogLevel(APP_LIBIO, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_MDLUTIL"))) {
                Lib_LogLevel(APP_LIBMDLUTIL, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_DYN"))) {
                Lib_LogLevel(APP_LIBDYN, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_PHY"))) {
                Lib_LogLevel(APP_LIBPHY, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_MIDAS"))) {
                Lib_LogLevel(APP_LIBMIDAS, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_EER"))) {
                Lib_LogLevel(APP_LIBEER, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_TDPACK"))) {
                Lib_LogLevel(APP_LIBTDPACK, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_MACH"))) {
                Lib_LogLevel(APP_LIBMACH, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_SPSDYN"))) {
                Lib_LogLevel(APP_LIBSPSDYN, envVarVal);
            }
            if ((envVarVal = getenv("APP_VERBOSE_META"))) {
                Lib_LogLevel(APP_LIBMETA, envVarVal);
            }

            // Check the language in the environment
            if ((envVarVal = getenv("CMCLNG"))) {
                App->Language = (envVarVal[0] == 'f' || envVarVal[0] == 'F') ? APP_FR : APP_EN;
            }
        }
    } // end OMP critical
    pthread_mutex_unlock(&App_mutex);
}

//! Initialize an application
TApp * App_Init(
    //! [in] Application type (APP_MASTER = single independent process, APP_THREAD = threaded co-process)
    const int Type,
    //! [in] Application name
    const char * const Name,
    //! [in] Application version
    const char * const Version,
    //! [in] Application description
    const char * const Desc,
    //! [in] TimeStamp
    const char * const Stamp
) {
    // In coprocess threaded mode, we need a different App object than the master thread
    App = (Type == APP_THREAD) ? (TApp*)calloc(1, sizeof(TApp)) : &AppInstance;

    if (App) {
        App->Type = Type;
        App->Name = Name ? strdup(Name) : strdup("");
        App->Version = Version ? strdup(Version) : strdup("");
        App->Desc = Desc ? strdup(Desc) : strdup("");
        App->TimeStamp = Stamp ? strdup(Stamp) : strdup("");
        App->LogFile = strdup("stderr");
        App->LogStream = (FILE*)NULL;
        App->Tag = NULL;
        App->State = APP_STOP;
        App->Percent = 0.0;
        App->Step = 0;
        App->Affinity = APP_AFFINITY_NONE;
        App->NbThread = 0;
        App->NbMPI = 1;
        App->RankMPI = 0;
        App->NbNodeMPI = 1;
        App->NodeRankMPI = 0;
        App->CountsMPI = NULL;
        App->DisplsMPI = NULL;
        App->TotalsMPI = NULL;
        App->OMPSeed = NULL;
        App->Seed = time(NULL);
        App->Signal = 0;
        App->LogWarning = 0;
        App->LogError = 0;

#ifdef HAVE_MPI
        App->Comm = MPI_COMM_WORLD;
        App->NodeComm = MPI_COMM_NULL;
        App->NodeHeadComm = MPI_COMM_NULL;

        App->MainComm = MPI_COMM_NULL;
        App->WorldRank = -1;
        App->ComponentRank = -1;
        App->SelfComponent = NULL;
        App->NumComponents = 0;
        App->AllComponents = NULL;
        App->NbSets = 0;
        App->SizeSets = 0;
        App->Sets = NULL;
#endif

        App_InitEnv();

        // Trap signals if enabled (preemption)
        if (App->Signal == 0) {
            App_Trap(SIGUSR2);
            App_Trap(SIGTERM);
        }
    } else {
        // This can only happen in thread mode so reassgin global instance for log message and revert to NULL
        App = &AppInstance;
        App_Log(APP_FATAL, "%s: Unable to allocate App internal structure\n", __func__);
        App = NULL;
    }

    //! \return Initialized application object pointer, or NULL on error
    return App;
}


//! Liberer les ressources de l'App
void App_Free(void) {
    pthread_mutex_lock(&App_mutex);
    {
        // only do it if not done already
        if (App->Name) {
            free(App->Name);
            App->Name = NULL;
            free(App->Version);
            free(App->Desc);
            free(App->LogFile);
            free(App->TimeStamp);

            APP_FREE(App->Tag);

            for(int t = 1; t < APP_LIBSMAX; t++) {
                APP_FREE(App->LibsVersion[t]);
            }

            APP_FREE(App->CountsMPI);
            APP_FREE(App->DisplsMPI);
            APP_FREE(App->OMPSeed);
        }

        // In coprocess threaded mode, we have a different App object than the master thread
        if (App->Type == APP_THREAD) {
            free(App);
            App = NULL;
        }

        //! \todo MPI stuff (MPMD)
    } // end OMP critical
    pthread_mutex_unlock(&App_mutex);
}

//! Initialiser les communicateurs intra-node et inter-nodes
int App_NodeGroup() {
    //! \note On fait ca ici car quand on combine MPI et OpenMP, les threads se supperpose sur un meme CPU pour plusieurs job MPI sur un meme "socket"
    if ( App_IsMPI() ) {
#ifdef HAVE_MPI
        // Get the physical node unique name of mpi procs
        char *names;
        APP_MEM_ASRT(names, calloc(MPI_MAX_PROCESSOR_NAME * App->NbMPI, sizeof(*names)));

        char *n = names + App->RankMPI * MPI_MAX_PROCESSOR_NAME;
        int nameLen;
        APP_MPI_ASRT( MPI_Get_processor_name(n, &nameLen) );
        APP_MPI_ASRT( MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, names, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, App->Comm) );

        // Go through the names and check how many different nodes we have before our node
        int color;
        char *cptr;
        int i;
        for (i = 0, color = -1, cptr = names; i <= App->RankMPI; ++i, cptr += MPI_MAX_PROCESSOR_NAME) {
            ++color;
            if ( !strncmp(n, cptr, MPI_MAX_PROCESSOR_NAME) ) {
                break;
            }
        }

        // Check if we have more than one group
        int mult = color;
        for(cptr = names; !mult && i < App->NbMPI; ++i, cptr += MPI_MAX_PROCESSOR_NAME) {
            if( strncmp(n, cptr, MPI_MAX_PROCESSOR_NAME) ) {
                mult = 1;
            }
        }

        // If we have more than one node
        if( mult ) {
            // Split the MPI procs into node groups
            APP_MPI_ASRT( MPI_Comm_split(App->Comm, color, App->RankMPI, &App->NodeComm) );

            // Get the number and rank of each nodes in this new group
            APP_MPI_ASRT( MPI_Comm_rank(App->NodeComm, &App->NodeRankMPI) );
            APP_MPI_ASRT( MPI_Comm_size(App->NodeComm, &App->NbNodeMPI) );

            // Create a communicator for the head process of each node
            APP_MPI_ASRT( MPI_Comm_split(App->Comm, App->NodeRankMPI?MPI_UNDEFINED:0, App->RankMPI, &App->NodeHeadComm) );
        } else {
            App->NbNodeMPI = App->NbMPI;
            App->NodeRankMPI = App->RankMPI;
            App->NodeComm = App->Comm;
            App->NodeHeadComm = MPI_COMM_NULL;
        }
#endif //HAVE_MPI
    } else {
        App->NbNodeMPI = App->NbMPI;
        App->NodeRankMPI = App->RankMPI;
#ifdef HAVE_MPI
        App->NodeComm = MPI_COMM_NULL;
        App->NodeHeadComm = MPI_COMM_NULL;
#endif //HAVE_MPI
    }

    return APP_OK;
}

int App_NodePrint() {
#ifdef HAVE_MPI
    if (App_IsMPI()) {
        if (!App->RankMPI) {
            char *nodes = calloc(MPI_MAX_PROCESSOR_NAME * App->NbMPI, sizeof(*nodes));
            if ( nodes ) {
                // Get the physical node unique name of mpi procs
                int nameLen = 0;
                APP_MPI_CHK( MPI_Get_processor_name(nodes, &nameLen) );
                APP_MPI_CHK( MPI_Gather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, nodes, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, App->Comm) );

                // Sort the names
                qsort(nodes, App->NbMPI, MPI_MAX_PROCESSOR_NAME, App_MPIProcCmp);

                // Print the node names with a count of MPI per nodes
                App_Log(APP_VERBATIM, "MPI nodes      :");
                char *n;
                int cnt;
                int i;
                for (i = 1, cnt = 1, n = nodes; i < App->NbMPI; ++i, n += MPI_MAX_PROCESSOR_NAME) {
                    if ( strncmp(n, n + MPI_MAX_PROCESSOR_NAME, MPI_MAX_PROCESSOR_NAME) ) {
                        App_Log(APP_VERBATIM, "%s%.*s (%d)", i != cnt ? ", " : " ", (int)MPI_MAX_PROCESSOR_NAME, n, cnt);
                        cnt = 1;
                    } else {
                        ++cnt;
                    }
                }
                App_Log(APP_VERBATIM, "%s%.*s (%d)\n", i != cnt ? ", " : " ", (int)MPI_MAX_PROCESSOR_NAME, n, cnt);

                free(nodes);
            }
        } else {
             // Send the node name (hostname)
            char node[MPI_MAX_PROCESSOR_NAME] = {'\0'};
            int nameLen = 0;
            APP_MPI_CHK( MPI_Get_processor_name(node, &nameLen) );
            APP_MPI_CHK( MPI_Gather(node, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, NULL, 0, MPI_DATATYPE_NULL, 0, App->Comm) );
        }

        // Allow the master node time to print the list uninterrupted
        APP_MPI_CHK( MPI_Barrier(App->Comm) );
    }
#endif
    return APP_OK;
}

//! Initialiser l'emplacement des threads
int App_ThreadPlace(void) {
    //! \note On fait ca ici car quand on combine MPI et OpenMP, les threads se supperpose sur un meme CPU pour plusieurs job MPI sur un meme "socket"

#ifndef _AIX
    // No thread affinity request
    if (!App->Affinity) {
        return TRUE;
    }

#ifdef HAVE_OPENMP
#ifndef __APPLE__
    if (App->NbThread > 1) {
        int nbcpu = sysconf(_SC_NPROCESSORS_ONLN);   // Get number of available  cores
        int incmpi = nbcpu / App->NbMPI;               // Number of cores per MPI job

        #pragma omp parallel
        {
            // unsigned int nid = omp_get_thread_num();
            pid_t tid = (pid_t) syscall(SYS_gettid);

            cpu_set_t set;
            CPU_ZERO(&set);

            switch(App->Affinity) {
                case APP_AFFINITY_SCATTER:
                    // Scatter threads evenly across all processors
                    CPU_SET((App->NodeRankMPI * incmpi) + omp_get_thread_num() * incmpi / App->NbThread, &set);
                    break;

                case APP_AFFINITY_COMPACT:
                    // Place threads closely packed
                    CPU_SET((App->NodeRankMPI * App->NbThread) + omp_get_thread_num(), &set);
                    break;

                case APP_AFFINITY_SOCKET:
                    // Pack threads over scattered MPI (hope it fits with sockets)
                    CPU_SET((App->NodeRankMPI*incmpi) + omp_get_thread_num(), &set);
                    break;

                case APP_AFFINITY_NONE:
                    break;
            }
            sched_setaffinity(tid, sizeof(set), &set);
        }
    }
#else
#warning "App: Setting affinity is not yet implemented for Apple App_ThreadPlace will be a no-op"
#endif
#endif
#endif
    return TRUE;
}

//! Initialiser l'execution de l'application et afficher l'entete
void App_Start(void) {
    App->State = APP_RUN;

    gettimeofday(&App->Time, NULL);

#ifdef HAVE_MPI
    App_SetMPIComm(App->Comm);
#endif

#ifdef HAVE_OPENMP
    // Initialize OpenMP
    if (App->NbThread) {
        // If a number of thread was specified on the command line
        omp_set_num_threads(App->NbThread);
    } else {
        // Otherwise try to get it from the environement
        const char * const env = getenv("OMP_NUM_THREADS");
        if (env) {
            App->NbThread = atoi(env);
        } else {
            App->NbThread = 1;
            omp_set_num_threads(0);
        }
    }

    // We need to initialize the per thread app pointer
    #pragma omp parallel
    {
        App = &AppInstance;
    }
    App_ThreadPlace();
#else
    App->NbThread = 1;
#endif

    // Modify seed value for current processor/thread for parallelization.
    App->OMPSeed = (int*)calloc(App->NbThread, sizeof(int));
#ifdef HAVE_RMN
    App_LibRegister(APP_LIBRMN, HAVE_RMN);
#endif

#ifdef HAVE_MPI
    if (!App->RankMPI || !App->ComponentRank) {
#endif
        if (!App->LogNoBox) {
            App_Log(APP_VERBATIM, "-------------------------------------------------------------------------------------\n");
            App_Log(APP_VERBATIM, "Application    : %s %s (%s)\n", App->Name, App->Version, App->TimeStamp);

            int libHeaderPrinted = FALSE;
            for(int libIdx = 1; libIdx < APP_LIBSMAX; libIdx++) {
                if (App->LibsVersion[libIdx]) {
                    if (!libHeaderPrinted) {
                        App_Log(APP_VERBATIM, "Libraries      :\n");
                        libHeaderPrinted = TRUE;
                    }
                    App_Log(APP_VERBATIM, "   %-12s: %s\n", AppLibNames[libIdx], App->LibsVersion[libIdx]);
                }
            }

            if (App->UTC) {
                App_Log(APP_VERBATIM, "\nStart time     : (UTC) %s", asctime(gmtime(&App->Time.tv_sec)));
            } else {
                App_Log(APP_VERBATIM, "\nStart time     : %s", ctime(&App->Time.tv_sec));
            }

#ifdef HAVE_OPENMP
            if (App->NbThread > 1) {
                // OpenMP specification version
                if       (_OPENMP >= 202411)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s6.0)\n", App->NbThread, _OPENMP, _OPENMP > 202411?" > ":"");
                else if  (_OPENMP >= 202111)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s5.2)\n", App->NbThread, _OPENMP, _OPENMP > 202111?" > ":"");
                else if  (_OPENMP >= 201811)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s5.0)\n", App->NbThread, _OPENMP, _OPENMP > 201811?" > ":"");
                else if  (_OPENMP >= 201511)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s4.5)\n", App->NbThread, _OPENMP, _OPENMP > 201511?" > ":"");
                else if  (_OPENMP >= 201307)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s4.0)\n", App->NbThread, _OPENMP, _OPENMP > 201307?" > ":"");
                else if  (_OPENMP >= 201107)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s3.1)\n", App->NbThread, _OPENMP, _OPENMP > 201107?" > ":"");
                else if  (_OPENMP >= 200805)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s3.0)\n", App->NbThread, _OPENMP, _OPENMP > 200805?" > ":"");
                else if  (_OPENMP >= 200505)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s2.5)\n", App->NbThread, _OPENMP, _OPENMP > 200505?" > ":"");
                else if  (_OPENMP >= 200203)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s2.0)\n", App->NbThread, _OPENMP, _OPENMP > 200203?" > ":"");
                else if  (_OPENMP >= 199810)  App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d -- OpenMP %s1.0)\n", App->NbThread, _OPENMP, _OPENMP > 199810?" > ":"");
                else                          App_Log(APP_VERBATIM, "OpenMP threads : %i (Standard: %d)\n", App->NbThread, _OPENMP);
            }
#endif //HAVE_OPENMP

#ifdef HAVE_MPI
            if (App->NbMPI > 1) {
#if defined MPI_VERSION && defined MPI_SUBVERSION
                // MPI specification version
                App_Log(APP_VERBATIM, "MPI processes  : %i (Standard: %d.%d)\n", App->NbMPI, MPI_VERSION, MPI_SUBVERSION);
#else
                App_Log(APP_VERBATIM, "MPI processes  : %i\n", App->NbMPI);
#endif
            }
#endif //HAVE_MPI
            App_Log(APP_VERBATIM, "-------------------------------------------------------------------------------------\n\n");
        }
#ifdef HAVE_MPI
    }
#endif

#ifdef HAVE_MPI
    // Make sure the header is printed before any other messages from other MPI tasks

    //! GEM with the IO-server (MPDP) uses App, but not App's MPDP laucher. Therefore,
    //! `MPI_Barrier(App->Comm)` will in definitely hang since not all PEs will execute
    //! it and since `App_MPMD_Init()` isn't called, App->Comm is still MPI_COMM_WORLD

    //! \todo Enable this only once GEM with the IO-server uses the App_MPMD launcher

    // if (App->NbMPI > 1 && mpiIsInit) {
    //     MPI_Barrier(App->Comm);
    // }
#endif //HAVE_MPI

    if (App->LogLevel[APP_MAIN]>=APP_STAT) {
        struct utsname buffer;
        if (uname(&buffer) ==0) {
            App_Log(APP_STAT, "System name: %s, Node name: %s, Release: %s, Version: %s, Machine: %s\n", 
                buffer.sysname,buffer.nodename,buffer.release,buffer.version,buffer.machine);
        }
    }
}


//! Finaliser l'execution du modele et afficher le footer
//! \return Process exit status
int App_Stats(const char * const Tag) {
    struct rusage  usg;
    struct timeval end,dif;

    if (App->LogLevel[APP_MAIN]>=APP_STAT) {
        gettimeofday(&end, NULL);
        timersub(&end, &App->Time, &dif);
        getrusage(RUSAGE_SELF, &usg);

        if (Tag) {
           App_Log(APP_STAT, ":%s: Elapsed: %.3f, User: %.3f, System: %.3f, RSS: %d Swap: %d, MinorFLT: %d, MajorFLT: %d\n",
              Tag,dif.tv_sec+dif.tv_usec/1e6,usg.ru_utime.tv_sec+usg.ru_utime.tv_usec/1e6,usg.ru_stime.tv_sec+usg.ru_stime.tv_usec/1e6,usg.ru_maxrss,usg.ru_nswap,usg.ru_minflt,usg.ru_majflt);
        } else {
           App_Log(APP_STAT, "Elapsed: %.3f, User: %.3f, System: %.3f, RSS: %d Swap: %d, MinorFLT: %d, MajorFLT: %d\n",
              dif.tv_sec+dif.tv_usec/1e6,usg.ru_utime.tv_sec+usg.ru_utime.tv_usec/1e6,usg.ru_stime.tv_sec+usg.ru_stime.tv_usec/1e6,usg.ru_maxrss,usg.ru_nswap,usg.ru_minflt,usg.ru_majflt);
        }
    }
    return(TRUE);
}

//! Finaliser l'execution du modele et afficher le footer
//! \return Process exit status
int App_End(
    //! Application exit status to use (-1:Use error count)
    int Status
) {
    //! \return Process exit status

    struct rusage usg;
    getrusage(RUSAGE_SELF, &usg);
    unsigned long * const mem = (unsigned long*)calloc(2 * App->NbMPI, sizeof(unsigned long));
    unsigned long * const memt = &mem[App->NbMPI];
    double sum = mem[App->RankMPI] = usg.ru_maxrss;

    // Get a readable size and units
    double factor = 1.0 / 1024;
    char * unit = AppMemUnits[1];

    double avg = 0.0, var = 0.0, maxd = 0.0, mind = 0.0, fijk = 0.0;
    unsigned int imin = 0, imax = 0;
#ifdef HAVE_MPI
    // The Status = INT_MIN means something went wrong and we want to crash gracefully and NOT get stuck
    // on a MPI deadlock where we wait for a reduce and the other nodes are stuck on a BCast, for example
    if (App->NbMPI > 1 && Status != INT_MIN) {
        if( !App->RankMPI) {
            MPI_Reduce(MPI_IN_PLACE, &App->LogWarning, 1, MPI_INT, MPI_SUM, 0, App->Comm);
            MPI_Reduce(MPI_IN_PLACE, &App->LogError, 1, MPI_INT, MPI_SUM, 0, App->Comm);
        } else {
            MPI_Reduce(&App->LogWarning, NULL, 1, MPI_INT, MPI_SUM, 0, App->Comm);
            MPI_Reduce(&App->LogError, NULL, 1, MPI_INT, MPI_SUM, 0, App->Comm);
        }

        // Calculate resident memory statistics
        MPI_Reduce(mem, memt, App->NbMPI, MPI_UNSIGNED_LONG, MPI_SUM, 0, App->Comm);

        if (!App->RankMPI) {
            double sumd2 = 0.0;
            sum  = 0.0;
            imin = 0;
            imax = App->NbMPI-1;
            maxd = memt[App->NbMPI-1];
            mind = memt[0];

            for(int i = 0; i < App->NbMPI; i++) {
                fijk  = memt[i];
                sum   = sum   + fijk;
                sumd2 = sumd2 + fijk * fijk;
                if (fijk > maxd) {
                    maxd = fijk;
                    imax = i;
                }
                if (fijk < mind) {
                    mind = fijk;
                    imin = i;
                }
            }

            avg = sum / App->NbMPI;
            var = sqrt((sumd2 + avg * avg * App->NbMPI - 2 * avg * sum) / App->NbMPI);

            if (sum > 1024 * 1024 * 10) {
                factor /= 1024;
                unit = AppMemUnits[2];
            }
        }
    }
#endif
    // Select status code based on error number
    if (Status < 0) {
        Status = App->LogError ? EXIT_FAILURE : EXIT_SUCCESS;
    }

#ifdef HAVE_MPI
    if (!App->RankMPI || !App->ComponentRank) {
#endif
        if (!App->LogNoBox) {
            struct timeval end;
            gettimeofday(&end, NULL);
            struct timeval dif;
            timersub(&end, &App->Time, &dif);

            App_Log(APP_VERBATIM, "\n-------------------------------------------------------------------------------------\n");
            App_Log(APP_VERBATIM, "Application    : %s %s (%s)\n\n", App->Name, App->Version, App->TimeStamp);
            if (App->Signal>0) {
                App_Log(APP_VERBATIM, "Trapped signal : %i\n", App->Signal);
            }
            if (App->UTC) {
                App_Log(APP_VERBATIM, "Finish time    : (UTC) %s", asctime(gmtime(&end.tv_sec)));
            } else {
                App_Log(APP_VERBATIM, "Finish time    : %s", ctime(&end.tv_sec));
            }
            App_Log(APP_VERBATIM, "Execution time : %.4f seconds (%.2f ms logging)\n", (float)dif.tv_sec+dif.tv_usec/1000000.0, App_TimerTotalTime_ms(App->TimerLog));
            App_Log(APP_VERBATIM, "Resident mem   : %.1f %s\n", sum*factor, unit);

            if (App->NbMPI>1) {
                App_Log(APP_VERBATIM, "   Average     : %.1f %s\n", avg * factor, unit);
                App_Log(APP_VERBATIM, "   Minimum     : %.1f %s (rank %u)\n", mind * factor, unit, imin);
                App_Log(APP_VERBATIM, "   Maximum     : %.1f %s (rank %u)\n", maxd * factor, unit, imax);
                App_Log(APP_VERBATIM, "   STD         : %.1f %s\n", var*factor, unit);

                for(int i = 0; i < App->NbMPI; i++) {
                    fijk  = memt[i];
                    if (fijk > (avg + var))
                       App_Log(APP_VERBATIM, "   Above 1 STD : %.1f %s (rank %u)\n", fijk * factor, unit, i);
                }
            }

            if (Status != EXIT_SUCCESS) {
                App_Log(APP_VERBATIM, "Status         : Error(code=%i) (%i Errors) (%i Warnings)\n", Status, App->LogError, App->LogWarning);
            } else if (App->LogError) {
                App_Log(APP_VERBATIM, "Status         : Ok (%i Errors) (%i Warnings)\n", App->LogError, App->LogWarning);
            } else if (App->LogWarning) {
                App_Log(APP_VERBATIM, "Status         : Ok (%i Warnings)\n", App->LogWarning);
            } else {
                App_Log(APP_VERBATIM, "Status         : Ok\n");
            }

            App_Log(APP_VERBATIM, "-------------------------------------------------------------------------------------\n");
        }
        App_LogClose();

        App->State = APP_DONE;
#ifdef HAVE_MPI
    }
#endif
    return (App->Signal>0) ? 128 + App->Signal : Status;
}


//! Trapper les signaux afin de terminer gracieusement
void App_TrapProcess(
    //! [in] Signal Signal to be trapped
    const int Signal
) {
    App_Log(APP_INFO, "Trapped signal %i\n", Signal);
    App->Signal = Signal;

    switch(Signal) {
        case SIGUSR2:
        case SIGTERM: App->State = APP_DONE;
    }
}

void App_Trap(const int Signal) {
    struct sigaction new;
    new.sa_sigaction = NULL;
    new.sa_handler = App_TrapProcess;
    new.sa_flags = 0x0;
    sigemptyset(&new.sa_mask);

    // POSIX way
    struct sigaction old;
    sigaction(Signal, &new, &old);

    // C Standard way
    // signal(Signal, App_TrapProcess);
}

void App_LogStream(const char * const Stream) {
      App->LogFile = strdup(Stream);
}

//! Ouvrir le fichier log
void App_LogOpen(void) {
    pthread_mutex_lock(&App_mutex);
    {
        if (!App->LogStream) {
            if (!App->LogFile || strcmp(App->LogFile, "stdout") == 0) {
                App->LogStream = stdout;
            } else if (strcmp(App->LogFile, "stderr") == 0) {
                App->LogStream = stderr;
            } else {
                if (!App->RankMPI) {
                    App->LogStream = fopen(App->LogFile, "w");
                } else {
                    App->LogStream = fopen(App->LogFile, "a+");
                }
            }
            if (!App->LogStream) {
                App->LogStream = stdout;
                fprintf(stderr, "(WARNING) Unable to open log stream (%s), will use stdout instead\n", App->LogFile);
            }

            // Split log file per MPI rank
            const int maxFilePathLen = 4096;
            char file[maxFilePathLen];
            if (App->LogSplit && App_IsMPI()) {
                snprintf(file, maxFilePathLen, "%s.%06d", App->LogFile, App->RankMPI);
                App->LogStream = freopen(file, "a", App->LogStream);
            }
        }
    } // end OMP critical
    pthread_mutex_unlock(&App_mutex);
}

//! Fermer le fichier log
void App_LogClose(void) {
    pthread_mutex_lock(&App_mutex);
    {
        fflush(App->LogStream);

        if (App->LogStream && App->LogStream != stdout && App->LogStream != stderr) {
            fclose(App->LogStream);
        }
    } // end OMP critical
    pthread_mutex_unlock(&App_mutex);
}

//! Imprimer un message de manière standard
void App_Log4Fortran(
    //! [in] Niveau d'importance du message (MUST, ALWAYS, FATAL, SYSTEM, ERROR, WARNING, INFO, DEBUG, EXTRA)
    TApp_LogLevel Level,
    //! [in] Message à jouter au journal
    const char *Message
) {
    Lib_Log(APP_MAIN, Level, "%s\n", Message);
}

void Lib_Log4Fortran(
    //! [in] Identificateur de la librairie
    TApp_Lib Lib,
    //! [in] Niveau d'importance du message (MUST, ALWAYS, FATAL, SYSTEM, ERROR, WARNING, INFO, DEBUG, EXTRA)
    TApp_LogLevel Level,
    //! [in] Message à jouter au journal
    char *Message,
    //! [in] Longueur du message
    int len
) {
    (void)len;
    Lib_Log(Lib, Level, "%s\n", Message);
}


//! Print message to log
void Lib_Log(
    //! [in] Library id
    const TApp_Lib lib,
    //! [in] Message level. See \ref TApp_LogLevel
    const TApp_LogLevel level,
    //! [in] printf style format string
    const char * const format,
    //! [in] Variables referrenced in the format string
    ...
) {
    //! \note If level is ERROR, the message will be written on stderr, for all other levels the message will be written to stdout or the log file

#ifdef HAVE_MPI
    if (App->LogRank != -1 && (App->LogRank != App->RankMPI && App->LogRank != App->ComponentRank)) {
        return;
    }
#endif
    // If not initialized yet
    if (!App->Tolerance) App_InitEnv();

    if (!App->LogStream) App_LogOpen();

    // Check for once log flag
    int effectiveLevel = level;
    if (effectiveLevel > APP_QUIET) {
        // If we logged it at least once
        if (effectiveLevel >> 3 < APP_MAXONCE && App_OnceTable[effectiveLevel >> 3]++) return;

        // Real log level
        effectiveLevel &= 0x7;
    }

    App_TimerStart(App->TimerLog);

    if (effectiveLevel == APP_WARNING) App->LogWarning++;
    if (effectiveLevel == APP_ERROR || effectiveLevel == APP_FATAL || effectiveLevel == APP_SYSTEM) App->LogError++;

    // Check if requested level is quiet
    if (App->LogLevel[lib] == APP_QUIET && effectiveLevel > APP_VERBATIM) return;

    // If this is within the request level
    if (effectiveLevel <= App->LogLevel[lib]) {
        char prefix[256];
        prefix[0] = '\0';
        if (effectiveLevel >= APP_ALWAYS) {
            char *color = App->LogColor ? AppLevelColors[effectiveLevel] : AppLevelColors[APP_INFO];

            char time[32];
            if (App->LogTime) {
                struct timeval now;
                gettimeofday(&now, NULL);

                struct tm *lctm;
                struct timeval diff;
                switch(App->LogTime) {
                case APP_DATETIME:
                    lctm = App->UTC ? gmtime(&now.tv_sec) : localtime(&now.tv_sec);
                    strftime(time, 32, "%c ", lctm);
                    break;
                case APP_TIME:
                    timersub(&now, &App->Time, &diff);
                    lctm = App->UTC ? gmtime(&diff.tv_sec) : localtime(&diff.tv_sec);
                    strftime(time, 32, "%T ", lctm);
                    break;
                case APP_SECOND:
                    timersub(&now, &App->Time, &diff);
                    snprintf(time, 32, "%-8.3f ", diff.tv_sec + diff.tv_usec / 1000000.0);
                    break;
                case APP_MSECOND:
                    timersub(&now, &App->Time, &diff);
                    snprintf(time, 32, "%-8li ", diff.tv_sec * 1000 + diff.tv_usec / 1000);
                    break;
                case APP_NODATE:
                    break;
                }
            } else {
                time[0] = '\0';
            }

#ifdef HAVE_MPI
           if (App_IsMPI() && App->LogRank == -1) {
                if (App->Step) {
                    sprintf(prefix, "%s%sP%03d (%s) #%d %s", color, time, App->RankMPI, AppLevelNames[effectiveLevel], App->Step, AppLibLog[lib]);
                } else {
                    sprintf(prefix, "%s%sP%03d (%s) %s", color, time, App->RankMPI, AppLevelNames[effectiveLevel], AppLibLog[lib]);
            }
                }
            else
#endif
            if (App->Step) {
                sprintf(prefix, "%s%s(%s) #%d %s", color, time, AppLevelNames[effectiveLevel], App->Step, AppLibLog[lib]);
            } else {
                sprintf(prefix, "%s%s(%s) %s", color, time, AppLevelNames[effectiveLevel], AppLibLog[lib]);
            }
        }

        va_list args;

        pthread_mutex_lock(&App_mutex);
        {
            fprintf(App->LogStream, "%s", prefix);

            va_start(args, format);
            vfprintf(App->LogStream, format, args);
            va_end(args);

            if (App->LogColor) {
                fprintf(App->LogStream, APP_COLOR_RESET);
            }

            // Force flush on error, when using colors or if APP_LOG_FLUSH flush is defined
            if (App->LogFlush || App->LogColor || effectiveLevel == APP_ERROR || effectiveLevel == APP_FATAL || effectiveLevel == APP_SYSTEM) {
                fflush(App->LogStream);
            }
        }
        pthread_mutex_unlock(&App_mutex);

        if (effectiveLevel == APP_ERROR || effectiveLevel == APP_FATAL || effectiveLevel == APP_SYSTEM) {
            // On errors, save for extenal to use (ex: Tcl)
            va_start(args, format);
            vsnprintf(APP_LASTERROR, APP_ERRORSIZE, format, args);
            va_end(args);

            // On system error
            if (effectiveLevel == APP_SYSTEM) {
                perror(APP_LASTERROR);
            }
        }
    }
    App_TimerStop(App->TimerLog);

    // Exit application if error above tolerance level
    if (App->Tolerance <= effectiveLevel && (effectiveLevel == APP_FATAL || effectiveLevel == APP_SYSTEM)) {
        exit(App_End(-1));
    }
}


//! Imprimer un message d'indication d'avancement
void App_Progress(
    //! [in] Pourcentage d'avancement
    const float Percent,
    //! [in] Message avec format à la printf
    const char * const Format,
    //! [in] Liste des variables du message
    ...
) {
    //! \note Cette fonction s'utilise comme printf sauf qu'il y a un argument de plus: le pourcentage d'avancement
    App->Percent = Percent;

    if (!App->LogStream) App_LogOpen();

    fprintf(App->LogStream, "%s(PROGRESS) [%6.2f %%] ", (App->LogColor?APP_COLOR_MAGENTA:""), App->Percent);
    va_list args;
    va_start(args, Format);
    vfprintf(App->LogStream, Format, args);
    va_end(args);

    if (App->LogColor) fprintf(App->LogStream, APP_COLOR_RESET);

    fflush(App->LogStream);
}


//! Define log level for application and libraries
int App_LogLevel(
    //! [in] Log level string ("FATAL", "SYSTEM", "ERROR", "WARNING", "INFO", "STAT", "TRIVIAL", "DEBUG", "EXTRA")
    const char * const level
) {
    //! \return Previous log level, or current if no level specified
    return Lib_LogLevel(APP_MAIN, level);
}


//! Definir le niveau de log courant pour une librairie
int Lib_LogLevel(
    //! [in] Library id
    const TApp_Lib lib,
    //! [in] Log level string ("FATAL", "SYSTEM", "ERROR", "WARNING", "INFO", "STAT", "TRIVIAL", "DEBUG", "EXTRA")
    const char * const level
) {
    //! The default log level is "WARNING". It can be changed with the APP_VERBOSE environment variable.
    //! Values for that variable must be the same as the ones provided to the level parameter of this function.

    // If not initialized yet
    if (!App->Tolerance){
        App_InitEnv();
    }

    // Keep previous level
    int previousLevel = App->LogLevel[lib];

    if (level && level[0] != ' ' && strlen(level)) {
        if (strncasecmp(level, "ERROR", 5) == 0) {
            App->LogLevel[lib] = APP_ERROR;
        } else if (strncasecmp(level, "WARN", 4) == 0) {
            App->LogLevel[lib] = APP_WARNING;
        } else if (strncasecmp(level, "INFO", 4) == 0) {
            App->LogLevel[lib] = APP_INFO;
        } else if (strncasecmp(level, "STAT", 4) == 0) {
            App->LogLevel[lib] = APP_STAT;
        } else if (strncasecmp(level, "TRIVIAL", 7) == 0) {
            App->LogLevel[lib] = APP_TRIVIAL;
        } else if (strncasecmp(level, "DEBUG", 5) == 0) {
            App->LogLevel[lib] = APP_DEBUG;
        } else if (strncasecmp(level, "EXTRA", 5) == 0) {
            App->LogLevel[lib] = APP_EXTRA;
        } else if (strncasecmp(level, "QUIET", 5) == 0) {
            App->LogLevel[lib] = APP_QUIET;
        } else {
            char *endptr = NULL;
            App->LogLevel[lib] = strtoul(level, &endptr, 10);
        }
        if (lib == APP_MAIN) {
            for(int l = 1; l < APP_LIBSMAX; l++) App->LogLevel[l] = App->LogLevel[APP_MAIN];
        }
    }
    //! \return Previous log level
    return previousLevel;
}

//! Definir le niveau de log courant pour l'application
int App_LogLevelNo(
    //! [in] Niveau de log
    const TApp_LogLevel Level
) {
    //! \return Previous log level, or current if no level specified
    return Lib_LogLevelNo(APP_MAIN, Level);
}

//! Set the rank of the MPI process that will display messages
int App_LogRank(
    //! [in] Rank of the MPI process that should display messages. -1 for all processes.
    const int NewRank
) {
    const int old_rank = App->LogRank;
    if (NewRank >= -1 && NewRank < App->NbMPI) {
        App->LogRank = NewRank;
    }
    //! \return Rank of the old MPI process that displayed the messages
    return old_rank;
}

//! Set the log level
int Lib_LogLevelNo(
    //! [in] Library id
    const TApp_Lib Lib,
    //! [in] Log level to process
    const TApp_LogLevel Level
) {
    // Save previous level
    int pl = App->LogLevel[Lib];

    // If not initialized yet
    if (!App->Tolerance) App_InitEnv();

    if (Level >= APP_FATAL && Level <= APP_QUIET) {
        App->LogLevel[Lib] = Level;
    }

    if (Lib == APP_MAIN) {
        for(int l = 1; l < APP_LIBSMAX; l++) App->LogLevel[l] = App->LogLevel[APP_MAIN];
    }

    //! \return Previous log level, or current if no level specified
    return pl;
}

//! Definir le niveau de tolerance aux erreur pour l'application
int App_ToleranceLevel(
    //! [in] Niveau de tolerance ("ERROR", "SYSTEM", "FATAL", "QUIET")
    const char * const Level
) {
    int pl = App->Tolerance;

    if (Level) {
        if (strlen(Level)) {
            if (Level[0] != ' ') {
                if (strncasecmp(Level, "ERROR", 5) == 0) {
                    App->Tolerance = APP_ERROR;
                } else if (strncasecmp(Level, "SYSTEM", 6) == 0) {
                    App->Tolerance = APP_SYSTEM;
                } else if (strncasecmp(Level, "FATAL", 5) == 0) {
                    App->Tolerance = APP_FATAL;
                } else if (strncasecmp(Level, "QUIET", 5) == 0) {
                    App->Tolerance = APP_QUIET;
                } else {
                    char *endptr = NULL;
                    App->Tolerance = strtoul(Level, &endptr, 10);
                }
            }
        }
    }

    //! \return Previous log level, or current if no level specified
    return pl;
}

//! Definir le niveau de tolerance aux erreur pour l'application
int App_ToleranceNo(
    //! [in] Niveau de tolerance (int)
    const TApp_LogLevel Level
) {
    int pl = App->Tolerance;
    if (Level >= APP_FATAL && Level <= APP_QUIET) App->Tolerance = Level;

    //! \return Previous log level, or current if no level specified
    return pl;
}

//! Definir le format du temps dans les log
int App_LogTime(
    //! [in] Niveau de détail temporel à afficher
    const char * const LogTime
) {
    int pf = App->LogTime;

    if (LogTime) {
        if (strcasecmp(LogTime, "NONE") == 0) {
            App->LogTime = APP_NODATE;
        } else if (strcasecmp(LogTime, "DATETIME") == 0) {
            App->LogTime = APP_DATETIME;
        } else if (strcasecmp(LogTime, "TIME") == 0) {
            App->LogTime = APP_TIME;
        } else if (strcasecmp(LogTime, "SECOND") == 0) {
            App->LogTime = APP_SECOND;
        } else if (strcasecmp(LogTime, "MSECOND") == 0) {
            App->LogTime = APP_MSECOND;
        } else {
            App->LogTime = (TApp_LogTime)atoi(LogTime);
        }
    }

    //! \return Previous time format, or current if no level specified
    return pf;
}

//! Print arguments information
void App_PrintArgs(
    //! [in] Arguments definition
    const TApp_Arg * const AArgs,
    //! [in] Invalid token if any, NULL otherwise
    const char * const Token,
    //! [in] Configuration flags
    int Flags
) {
    printf("%s (%s):\n\t%s\n\n", App->Name, App->Version, App->Desc);

    if (Token) printf("Bad option: %s\n\n", Token);

    printf("Usage:");

    // Process app specific argument
    const TApp_Arg *aarg = AArgs;
    while(aarg && aarg->Short) {
        if (aarg->Short[0] == '\0') {
            printf("\n\t    --%-15s %s", aarg->Long, aarg->Info);
        } else {
            printf("\n\t-%s, --%-15s %s", aarg->Short, aarg->Long, aarg->Info);
        }
        aarg++;
    }

    // Process default argument
    if (Flags & APP_ARGSSEED)   printf("\n\t-%s, --%-15s %s", "s", "seed",     "Seed (FIXED, "APP_COLOR_GREEN"VARIABLE"APP_COLOR_RESET" or seed)");
    if (Flags & APP_ARGSTHREAD) printf("\n\t-%s, --%-15s %s", "t", "threads",     "Number of threads ("APP_COLOR_GREEN"0"APP_COLOR_RESET")");
    if (Flags & APP_ARGSTHREAD) printf("\n\t    --%-15s %s", "affinity",     "Thread affinity ("APP_COLOR_GREEN"NONE"APP_COLOR_RESET", COMPACT, SCATTER, SOCKET)");

    printf("\n");
    if (Flags & APP_ARGSLOG)    printf("\n\t-%s, --%-15s %s", "l", "log",     "Log file (stdout, "APP_COLOR_GREEN"stderr"APP_COLOR_RESET", file)");
    if (Flags & APP_ARGSLOG)    printf("\n\t    --%-15s %s",      "logsplit", "Split log file per MPI rank");
    if (Flags & APP_ARGSLANG)   printf("\n\t-%s, --%-15s %s", "a", "language", "Language ("APP_COLOR_GREEN"$CMCLNG"APP_COLOR_RESET", english, francais)");

    printf("\n\t-%s, --%-15s %s", "v", "verbose",      "Verbose level (ERROR, "APP_COLOR_GREEN"WARNING"APP_COLOR_RESET", INFO, DEBUG, EXTRA, QUIET)");
    printf("\n\t    --%-15s %s",      "verbosetime",  "Display time in logs ("APP_COLOR_GREEN"NONE"APP_COLOR_RESET", DATETIME, TIME, SECOND, MSECOND)");
    printf("\n\t    --%-15s %s",      "verboseutc", "Use UTC for time");
    printf("\n\t    --%-15s %s",      "verbosecolor", "Use color for log messages");
    printf("\n\t-%s, --%-15s %s", "h", "help",         "Help info");
    printf("\n");
}

#define LST_ASSIGN(type, lst, val) *(type)lst = val; lst = (type)lst + 1
//! Extract argument value
static inline int App_GetArgs(
    //! [in, out] Argument definition
    TApp_Arg * const AArg,
    //! [in] Value to extract
    char * const Value
) {
    char *endptr = NULL;

    if (Value) {
        if ((--AArg->Multi) < 0) {
            printf("Too many values for parametre -%s, --%s\n", AArg->Short, AArg->Long);
            exit(EXIT_FAILURE);
        }

        switch(AArg->Type & (~APP_FLAG)) {
            case APP_CHAR :  LST_ASSIGN(char**,         AArg->Var, Value);                      break;
            case APP_UINT32: LST_ASSIGN(unsigned int*,  AArg->Var, strtol(Value, &endptr, 10)); break;
            case APP_INT32:  LST_ASSIGN(int*,           AArg->Var, strtol(Value, &endptr, 10)); break;
            case APP_UINT64: LST_ASSIGN(unsigned long*, AArg->Var, strtol(Value, &endptr, 10)); break;
            case APP_INT64:  LST_ASSIGN(long*,          AArg->Var, strtol(Value, &endptr, 10)); break;
            case APP_FLOAT32:LST_ASSIGN(float*,         AArg->Var, strtof(Value, &endptr));     break;
            case APP_FLOAT64:LST_ASSIGN(double*,        AArg->Var, strtod(Value, &endptr));     break;
        }
    } else {
        if (AArg->Type & APP_FLAG) *(int*)AArg->Var = 0x01;
    }
    if (!(AArg->Type & APP_FLAG)  && (!Value || (endptr && endptr == Value))) {
        printf("Invalid value for parametre -%s, --%s: %s\n", AArg->Short, AArg->Long, Value);
        exit(EXIT_FAILURE);
    }
    //! \return 1 on succes, but quits the process on error
    return 1;
}

//! Parse default arguments
int App_ParseArgs(
    //! [in] Argument definition
    TApp_Arg *AArgs,
    //! [in] Number of argument
    int argc,
    //! [in] Arguments
    char *argv[],
    //! [in] Configuration flags
    int Flags
) {
    //! \return 1 or 0 if failed

    int ok = TRUE;
    int ner = TRUE;
    char *env = NULL;
    char *str;

    str = env = getenv("APP_PARAMS");

    if (argc == 1 && !env && Flags & APP_NOARGSFAIL) {
        App_PrintArgs(AArgs, NULL, Flags) ;
        ok = FALSE;
    } else {
        // Parse parameters either on command line or through environment variable
        TApp_Arg *aarg = NULL;
        char *tok, *ptok = NULL, *endptr = NULL, *tmp;
        int i = 1;
        while((i < argc && (tok = argv[i])) || (env && (tok = strtok(str, " ")))) {
            str = NULL;

            // Check if token is a flag or a value (for multi-value parameters)
            if (tok[0] != '-' && ptok) {
                tok = ptok;
                --i;
            }

            // Process default argument
            if ((Flags & APP_ARGSLANG) && (!strcasecmp(tok, "-a") || !strcasecmp(tok, "--language"))) {  // language (en, fr)
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    tmp = env ? strtok(str, " ") : argv[i];
                    if (tmp[0] == 'f' || tmp[0] == 'F') {
                        App->Language = APP_FR;
                    } else if  (tmp[0] == 'e' || tmp[0] == 'E') {
                        App->Language = APP_EN;
                    } else {
                        printf("Invalid value for language, must be francais or english\n");
                        exit(EXIT_FAILURE);
                    }
                }
            } else if ((Flags & APP_ARGSLOG) && (!strcasecmp(tok, "-l") || !strcasecmp(tok, "--log"))) { // Log file
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    free(App->LogFile);
                    App->LogFile = strdup(env ? strtok(str, " ") : argv[i]);
                }
            } else if ((Flags & APP_ARGSLOG) && !strcasecmp(tok, "--logsplit")) { // Log file split
                App->LogSplit = TRUE;
            } else if ((Flags & APP_ARGSTHREAD) && (!strcasecmp(tok, "-t") || !strcasecmp(tok, "--threads"))) { // Threads
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    tmp = env ? strtok(str, " ") : argv[i];
                    App->NbThread = strtol(tmp, &endptr, 10);
                }
            } else if ((Flags & APP_ARGSTHREAD) && !strcasecmp(tok, "--affinity")) { // Threads
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                tmp = env ? strtok(str, " ") : argv[i];
                if (!strcasecmp(tmp, "NONE")) {
                    App->Affinity = APP_AFFINITY_NONE;
                } else if (!strcasecmp(tmp, "COMPACT")) {
                    App->Affinity = APP_AFFINITY_COMPACT;
                } else if (!strcasecmp(tmp, "SCATTER")) {
                    App->Affinity = APP_AFFINITY_SCATTER;
                } else if (!strcasecmp(tmp, "SOCKET")) {
                    App->Affinity = APP_AFFINITY_SOCKET;
                } else {
                    printf("Invalid value for thread affinity, NONE, COMPACT, SCATTER or SOCKET\n");
                    exit(EXIT_FAILURE);
                }
                }
            // } else if ((Flags&APP_ARGSTMPDIR) && (!strcasecmp(tok, "--tmpdir"))) { // Use tmpdir if available
            //    i++;
            //    if ((ner = ok = (i<argc && argv[i][0] != '-'))) {
            //       App->r = env?strtok(str, " "):argv[i];
            //    }
            } else if ((Flags & APP_ARGSSEED) && (!strcasecmp(tok, "-s") || !strcasecmp(tok, "--seed"))) { // Seed
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    tmp = env ? strtok(str, " ") : argv[i];
                    if (strcasecmp(tmp, "VARIABLE") == 0 || strcmp(tmp, "1") == 0) {
                        // Seed is variable, according to number of elapsed seconds since January 1 1970, 00:00:00 UTC.
                    } else if (strcasecmp(tmp, "FIXED") == 0 || strcmp(tmp, "0") == 0) {
                        // Seed is fixed
                        App->Seed = APP_SEED;
                    } else {
                        // Seed is user defined
                        App->Seed = strtol(tmp, &endptr, 10);
                    }
                }
            } else if (!strcasecmp(tok, "-v") || !strcasecmp(tok, "--verbose")) {                      // Verbose degree
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    App_LogLevel(env ? strtok(str, " ") : argv[i]);
                }
            } else if (!strcasecmp(tok, "--verbosetime")) {                                           // Verbose time type
                i++;
                if ((ner = ok = (i < argc && argv[i][0] != '-'))) {
                    App_LogTime(env ? strtok(str, " ") : argv[i]);
                }
            } else if (!strcasecmp(tok, "--verboseutc")) {                                            // Use UTC time in log messages
                App->UTC = TRUE;
            } else if (!strcasecmp(tok, "--verbosecolor")) {                                          // Use color in log messages
                App->LogColor = TRUE;
            } else if (!strcasecmp(tok, "-h") || !strcasecmp(tok, "--help")) {                         // Help
                App_PrintArgs(AArgs, NULL, Flags) ;
                exit(EXIT_SUCCESS);
            } else {
                // Process specific argument
                aarg = AArgs;
                while(aarg->Short) {
                    if ((aarg->Short && tok[1] == aarg->Short[0] && tok[2] == '\0') || (aarg->Long && strcasecmp(&tok[2], aarg->Long) == 0)) {
                        ok = (aarg->Type == APP_FLAG ? (*(int*)aarg->Var) = TRUE : App_GetArgs(aarg, env ? strtok(str, " ") : ((i + 1 < argc && argv[i+1][0] != '-') ? argv[++i] : NULL)));
                        ptok = aarg->Type == APP_FLAG ? NULL : tok;
                        break;
                    }
                    aarg++;
                }
            }

            if (!ner) {
                printf("Missing argument for %s\n", argv[--i]);
                exit(EXIT_FAILURE);
            }

            // Argument not found
            if (aarg && (!ok || !aarg->Short)) {
                App_PrintArgs(AArgs, tok, Flags) ;
                ok = FALSE;
                break;
            }

            ++i;
        }
    }

    return ok;
}

//! Parse key value file
int App_ParseInput(
    //! [in] Model definitions
    void *Def,
    //! [in] Input file to parse
    char *File,
    //! [in] Model specific token parsing function
    TApp_InputParseProc *ParseProc
) {
    //! \return Number of token parsed or 0 if failed

    //! @note
    //! - This proc will parse an input file with the format TOKEN = VALUE
    //! - It will skip any comment and blank lines
    //! - It also allows for multiline definitions

    FILE *fp = fopen(File, "r");
    if (!fp) {
        App_Log(APP_ERROR, "Unable to open input file: %s\n", File);
        return 0;
    }

    char *buf = (char*)alloca(APP_BUFMAX);
    if (!buf) {
        App_Log(APP_ERROR, "Unable to allocate input parsing buffer\n");
        return 0;
    }

    int n = 0;
    int seq = 0;
    char token[256], *parse, *values, *value, *valuesave, *idx, *tokensave;
    while(fgets(buf, APP_BUFMAX, fp)) {
        //Check for comments
        strtrim(buf, '\n');
        strrep(buf, '\t', ' ');
        if ((idx = index(buf, '#'))) *idx = '\0';

        //Parse the token
        parse = NULL;
        if ((idx = index(buf, '='))) {
            tokensave = NULL;
            parse = strtok_r(buf, "=", &tokensave);
        }

        if (parse) {
            // If we find a token, remove spaces and get the associated value
            strtrim(parse, ' ');
            strncpy(token, parse, 255);
            values = strtok_r(NULL, "=", &tokensave);
            seq = 0;
            n++;
        } else {
            // Otherwise, keep the last token and get a new value for it
            values = buf;
            strtrim(values, ' ');
        }

        // Loop on possible space separated values
        if (values && strlen(values) > 1) {
            valuesave = NULL;
            while((value = strtok_r(values, " ", &valuesave))) {
                if (seq) {
                    App_Log(APP_DEBUG, "Input parameters: %s(%i) = %s\n", token, seq, value);
                } else {
                    App_Log(APP_DEBUG, "Input parameters: %s = %s\n", token, value);
                }

                // Call mode specific imput parser
                if (!ParseProc(Def, token, value, seq)) {
                    fclose(fp);
                    return 0;
                }
                seq++;
                values = NULL;
            }
        }
    }

    fclose(fp);
    return n;
}


//! Parse a boolean value
int App_ParseBool(
    //! [in] Parameter name
    char *Param,
    //! [in] Value to parse
    char *Value,
    //! [out] Variable into which to put the result
    char *Var
) {
    //! \return 1 on success, 0 otherwise

    if (strcasecmp(Value, "true") == 0 || strcmp(Value, "1") == 0) {
        *Var = 1;
    } else if (strcasecmp(Value, "false") == 0 || strcmp(Value, "0") == 0) {
        *Var = 0;
    } else {
        App_Log(APP_ERROR, "Invalid value for %s, must be TRUE(1) or FALSE(0): %s\n", Param, Value);
        return 0;
    }
    return 1;
}


//! Convert date time to seconds
time_t App_DateTime2Seconds(
    //! [in] Date in YYYMMDD format
    int YYYYMMDD,
    //! [in] Time in HHMMSS format
    int HHMMSS,
    //! [in] If true the provided date and time will be interpreted as GMT instead of local
    int GMT
) {
    //! \return Date time in seconds

    struct tm date;

    date.tm_sec = fmod(HHMMSS, 100);       // seconds apres la minute [0, 61]
    HHMMSS /= 100;
    date.tm_min = fmod(HHMMSS, 100);       // minutes apres l'heure [0, 59]
    HHMMSS /= 100;
    date.tm_hour = HHMMSS;                 // heures depuis minuit [0, 23]

    date.tm_mday = fmod(YYYYMMDD, 100);    // jour du mois [1, 31]
    YYYYMMDD /= 100;
    date.tm_mon = fmod(YYYYMMDD, 100) - 1; // mois apres Janvier [0, 11]
    YYYYMMDD /= 100;
    date.tm_year = YYYYMMDD - 1900;         // annee depuis 1900
    date.tm_isdst = 0;                      // Flag de l'heure avancee

    // Force GMT and set back to original TZ after
    extern time_t timezone;
    if (GMT) {
        return mktime(&date) - timezone;
    } else {
        return mktime(&date);
    }
}


//! Parse a date in the YYYMMDDhhmm format
int App_ParseDate(
    //! [in] Parameter name
    char *Param,
    //! [in] Value to parse
    char *Value,
    //! [out] Variable into which to put the result
    time_t *Var
) {
    //! \return 1 on success, 0 otherwise

    char *ptr;
    long long t = strtoll(Value, &ptr, 10);
    if( t <= 0 ) {
        App_Log(APP_ERROR, "Invalid value for %s, must be YYYYMMDDHHMM or YYYYMMDDHHMMSS: %s\n", Param, Value);
        return 0;
    }
    switch( strlen(Value) ) {
        case 12:
            *Var = App_DateTime2Seconds(t / 10000, (t - (t / 10000 * 10000)) * 100, 1);
            break;
        case 14:
            *Var = App_DateTime2Seconds(t / 1000000, (t - (t / 1000000 * 1000000)), 1);
            break;
        default:
            App_Log(APP_ERROR, "Invalid value for %s, must be YYYYMMDDHHMM or YYYYMMDDHHMMSS: %s\n", Param, Value);
            return 0;
    }

    return 1;
}


// Parse a date value and return the split values
int App_ParseDateSplit(
    // [in] Parameter name
    const char * const param,
    // [in] Value to parse
    char * const value,
    // [out] Year
    int * const year,
    // [out] Month
    int * const month,
    // [out] Day
    int * const day,
    // [out] Hour
    int * const hour,
    // [out] Minutes
    int * const min
) {
    // \result 1 on success, 0 otherwise

    char * endptr;
    long long t = strtoll(value, &endptr, 10);
    if (strlen(value) != 12 || t <= 0) {
        App_Log(APP_ERROR, "Invalid value for %s, must be YYYYMMDDHHMM: %s\n", param, value);
        return 0;
    }
    *year  = t / 100000000;  t -= (long long)(*year) * 100000000;
    *month = t / 1000000;    t -= (*month) * 1000000;
    *day   = t / 10000;      t -= (*day) * 10000;
    *hour  = t / 100;        t -= (*hour) * 100;
    *min   = t;

    return 1;
}


//! Parse a coordinate value
int App_ParseCoords(
    //! [in] Parameter name
    const char * const Param,
    //! [in] Parameter value
    const char * const Value,
    //! [out] Latitude
    double * const Lat,
    //! [out] Longitude
    double * const Lon,
    //! [in] Coordinate type (0:Lat, 1:Lon)
    const int Index
) {
    //! \return 1 on success, 0 otherwise

    // Hack to remove unused parameter warning
    (void)(Param);

    char *ptr;
    double coord = strtod(Value, &ptr);

    switch(Index) {
        case 0:
            if (coord < -90.0 || coord > 90.0) {
                App_Log(APP_ERROR, "Invalid latitude coordinate: %s\n", Value);
                return 0;
            }
            *Lat = coord;
            break;
        case 1:
            if (coord<0.0) coord += 360.0;
            if (coord<0.0 || coord > 360.0) {
                App_Log(APP_ERROR, "Invalid longitude coordinate: %s\n", Value);
                return 0;
            }
            *Lon = coord;
            break;
    }

    return 1;
}


//! Initialize seeds for MPI/OpenMP
void App_SeedInit() {
    // Modify seed value for current processor/thread for parallelization.
    for(int t = 1; t < App->NbThread; t++) {
        App->OMPSeed[t] = App->Seed + 1000000 * (App->RankMPI * App->NbThread + t);
    }

    App->OMPSeed[0] = App->Seed += 1000000 * App->RankMPI;
}
