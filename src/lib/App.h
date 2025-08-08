#ifndef _App_h
#define _App_h

//! \file
//! Interface of the App library

#include <stdio.h>
#include <sys/time.h>

#include "App_Atomic.h"
#include "App_Timer.h"

#ifdef HAVE_OPENMP
#   include <omp.h>
#endif

#ifdef HAVE_MPI
#   include <mpi.h>
#include "App_Shared_Memory.h"
#endif

#ifndef TRUE
   #define TRUE   1
   #define FALSE  0
#endif

#define APP_COLOR_BLINK      "\x1b[5m"
#define APP_COLOR_BLACK      "\x1b[0;30m"
#define APP_COLOR_RED        "\x1b[0;31m"
#define APP_COLOR_GREEN      "\x1b[0;32m"
#define APP_COLOR_LIGHTGREEN "\x1b[1;32m"
#define APP_COLOR_ORANGE     "\x1b[33m" 
#define APP_COLOR_YELLOW     "\x1b[1m\x1b[33m"
#define APP_COLOR_BLUE       "\x1b[0;34m"
#define APP_COLOR_MAGENTA    "\x1b[0;35m"
#define APP_COLOR_CYAN       "\x1b[0;36m"
#define APP_COLOR_LIGHTCYAN  "\x1b[1m\x1b[36m"
#define APP_COLOR_GRAY       "\x1b[0;37m"
#define APP_COLOR_RESET      "\x1b[0m"
#define APP_MASTER    0
#define APP_THREAD    1

#define APP_ERRORSIZE 2048
#define APP_BUFMAX    32768               ///< Maximum input buffer length
#define APP_LISTMAX   4096                ///< Maximum number of items in a flag list
#define APP_SEED      1049731793          ///< Initial FIXED seed
#define APP_LIBSMAX   64                  ///< Maximum number of libraries

#define APP_NOARGSFLAG 0x00               ///< No flag specified
#define APP_NOARGSFAIL 0x01               ///< Fail if no arguments are specified
#define APP_ARGSLOG    0x02               ///< Use log flag
#define APP_ARGSLANG   0x04               ///< Multilingual app
#define APP_ARGSSEED   0x08               ///< Use seed flag
#define APP_ARGSTHREAD 0x10               ///< Use thread flag
#define APP_ARGSTMPDIR 0x20               ///< Use tmp dir

#ifdef __xlC__
#   define APP_ONCE    ((1)<<3)
#else
#   define APP_ONCE    ((__COUNTER__+1)<<3)
#endif
#define APP_MAXONCE 1024

//! Maximum component lane length (including null character)
#define APP_MAX_COMPONENT_NAME_LEN 32

//! List of known libraries
//! \todo Remove this static list; it makes no sense: this library must be modified to add new clients apps/libs!
//! The library must not be aware of its users!
typedef enum {
    //! Main application
    APP_MAIN = 0,
    APP_LIBRMN = 1,
    APP_LIBFST = 2,
    APP_LIBBRP = 3,
    APP_LIBWB = 4,
    APP_LIBGMM = 5,
    APP_LIBVGRID = 6,
    APP_LIBINTERPV = 7,
    APP_LIBGEOREF = 8,
    APP_LIBRPNMPI = 9,
    APP_LIBIRIS = 10,
    APP_LIBIO = 11,
    APP_LIBMDLUTIL = 12,
    APP_LIBDYN = 13,
    APP_LIBPHY = 14,
    APP_LIBMIDAS = 15,
    APP_LIBEER = 16,
    APP_LIBTDPACK = 17,
    APP_LIBMACH = 18,
    APP_LIBSPSDYN = 19,
    APP_LIBMETA = 20
} TApp_Lib;

//! Log levels
typedef enum {
    //! Written even if the selected level is quiet
    APP_VERBATIM = -1,
    APP_ALWAYS = 0,
    //! Fatal error. Will cause the application to be terminated.
    APP_FATAL = 1,
    //! System error. Will cause the application to be terminated.
    APP_SYSTEM = 2,
    //! Error. Written to stderr
    APP_ERROR = 3,
    //! Warning
    APP_WARNING = 4,
    //! Informational
    APP_INFO = 5,
    //! Stats about process
    APP_STAT = 6,
    //! Trivial
    APP_TRIVIAL = 7,
    //! Debug
    APP_DEBUG = 8,
    //! Extra
    APP_EXTRA = 9,
    //! Quiet \todo Say what quiet actually does
    APP_QUIET = 10
} TApp_LogLevel;

//! Log date detail level
typedef enum {
    APP_NODATE = 0,
    APP_DATETIME = 1,
    APP_TIME = 2,
    APP_SECOND = 3,
    APP_MSECOND = 4
} TApp_LogTime;

//! Application state
typedef enum {
    APP_STOP,
    APP_RUN,
    APP_DONE
} TApp_State;

//! Data type
typedef enum {
    APP_NIL = 0x00,
    APP_FLAG = 0x01,
    APP_CHAR = 0x02,
    APP_UINT32 = 0x04,
    APP_INT32 = 0x06,
    APP_UINT64 = 0x08,
    APP_INT64 = 0x0A,
    APP_FLOAT32 = 0x0C,
    APP_FLOAT64 = 0x0E
} TApp_Type;

//! Language
typedef enum {
    APP_FR = 0x00,
    APP_EN = 0x01
} TApp_Lang;

//! Function return code
typedef enum {
    APP_OK = 1,
    APP_ERR = 0
} TApp_RetCode;

//! Processor affinity
typedef enum {
    APP_AFFINITY_NONE = 0,
    APP_AFFINITY_COMPACT = 1,
    APP_AFFINITY_SCATTER = 2,
    APP_AFFINITY_SOCKET = 3
} TApp_Affinity;

#define APP_ASRT_OK(x) if( (x) != APP_OK ) return APP_ERR
#define APP_ASRT_OK_END(x) if( (x) != APP_OK ) goto end
#define APP_ASRT_OK_M(Fct, ...) \
    if( (Fct) != APP_OK ) { \
        Lib_Log(APP_MAIN, APP_ERROR, __VA_ARGS__); \
        return APP_ERR; \
    }

// Check FST function and return the specified value if an error was encountered
#define APP_FST_ASRT_H(Fct, ...) \
    if( (Fct) < 0 ) { \
        Lib_Log(APP_MAIN, APP_ERROR,  __VA_ARGS__); \
        return APP_ERR; \
    }
#define APP_FST_ASRT_H_END(Fct, ...) \
    if( (Fct) < 0 ) { \
        Lib_Log(APP_MAIN, APP_ERROR,  __VA_ARGS__); \
        goto end; \
    }
#define APP_FST_ASRT(Fct, ...) \
    if( (Fct) != 0 ) { \
        Lib_Log(APP_MAIN, APP_ERROR,  __VA_ARGS__); \
        return APP_ERR; \
    }
#define APP_FST_ASRT_END(Fct, ...) \
    if( (Fct) != 0 ) { \
        Lib_Log(APP_MAIN, APP_ERROR,  __VA_ARGS__); \
        goto end; \
    }
// Memory helpers
#define APP_MEM_ASRT(Buf, Fct) \
    if( !(Buf = (Fct)) ) { \
        Lib_Log(APP_MAIN, APP_ERROR, "(%s) Could not allocate memory for field %s at line %d.\n", __func__, #Buf, __LINE__); \
        return APP_ERR; \
    }
#define APP_MEM_ASRT_END(Buf, Fct) \
    if( !(Buf = (Fct)) ) { \
        Lib_Log(APP_MAIN, APP_ERROR, "(%s) Could not allocate memory for field %s at line %d.\n", __func__, #Buf, __LINE__); \
        goto end; \
    }

#define APP_FREE(Ptr) if(Ptr) { free(Ptr); Ptr = NULL; }

// MPI helpers
#ifdef HAVE_MPI
#define APP_MPI_ASRT(Fct) { \
    int err = (Fct); \
    if( err != MPI_SUCCESS ) { \
        Lib_Log(APP_MAIN, APP_ERROR, "(%s) MPI call %s at line %d failed with code %d for MPI node %d\n", __func__, #Fct, __LINE__, err, App->RankMPI); \
        return APP_ERR; \
    } \
}
#define APP_MPI_ASRT_END(Fct) { \
    int err = (Fct); \
    if( err != MPI_SUCCESS ) { \
        Lib_Log(APP_MAIN, APP_ERROR, "(%s) MPI call %s at line %d failed with code %d for MPI node %d\n", __func__, #Fct, __LINE__, err, App->RankMPI); \
        goto end; \
    } \
}
#define APP_MPI_CHK(Fct) { \
    int err = (Fct); \
    if( err != MPI_SUCCESS ) { \
        Lib_Log(APP_MAIN, APP_ERROR, "(%s) MPI call %s at line %d failed with code %d for MPI node %d\n", __func__, #Fct, __LINE__, err, App->RankMPI); \
    } \
}
#define APP_MPI_IN_PLACE(Fld) (App->RankMPI ? (Fld) : MPI_IN_PLACE)

//! MPDP component description
typedef struct {
    //! ID of this component, corresponds to MPI_APPNUM
    int id;
    //! Name of the component
    char name[APP_MAX_COMPONENT_NAME_LEN];
    //! Communicator for the PEs of this component
    MPI_Comm comm;
    //! Number of PEs in this component
    int nbPes;
    //! Whether this component has been shared to other PEs of this PE's component
    int shared;
    //! Global ranks (in the main_comm of the context) of the members of this component
    int * ranks;
} TComponent;

//! A series of components that share a communicator
typedef struct {
    //! How many components are in the set
    int nbComponents;
    //! IDs of the components in this set
    int* componentIds;
    //! Communicator shared by these components
    MPI_Comm comm;
    //! MPI group shared by these component, created for creating the communicator
    MPI_Group group;
} TComponentSet;

#endif //HAVE_MPI

//! Argument definitions
typedef struct {
    TApp_Type  Type;    //!< Argument type
    void      *Var;     //!< Where to put the argument value(s)
    int        Multi;   //!< Multiplicity of the argument (Maximum number of values for a list)
    char      *Short;   //!< Short description
    char      *Long;    //!< Long description
    char      *Info;    //!< Additional info
} TApp_Arg;

//! Application controller definition
typedef struct {
   char*          Name;                  ///< Name of applicaton
   char*          Version;               ///< Version of application
   char*          Desc;                  ///< Description of application
   char*          TimeStamp;             ///< Compilation timestamp
   char*          LogFile;               ///< Log file
   int            LogSplit;              ///< Split the log file per MPI rank path
   int            LogFlush;              ///< Forche buffer flush at every message
   char*          Tag;                   ///< Identificateur
   FILE*          LogStream;             ///< Log file associated stream
   int            LogNoBox;              ///< Display header and footer boxes
   int            LogRank;               ///< Force log from a single rank
   int            LogThread;             ///< Display thread id
   int            LogWarning;            ///< Number of warnings
   int            LogError;              ///< Number of errors
   int            LogColor;              ///< Use coloring in the logs
   TApp_LogTime   LogTime;               ///< Display time in the logs
   TApp_LogLevel  LogLevel[APP_LIBSMAX]; ///< Level of log
   TApp_LogLevel  Tolerance;             ///< Abort level
   TApp_State     State;                 ///< State of application
   TApp_Lang      Language;              ///< Language (default: $CMCLNG or APP_EN)
   double         Percent;               ///< Percentage of execution done (0=not started, 100=finished)
   int            UTC;                   ///< Use UTC or local time
   struct timeval Time;                  ///< Timer for execution time
   int            Type;                  ///< App object type (APP_MASTER, APP_THREAD)
   int            Step;                  ///< Model step

   char*          LibsVersion[APP_LIBSMAX];

   int            Seed;
   int           *OMPSeed;               ///< Random number generator seed
   int           *TotalsMPI;             ///< MPI total number of items arrays
   int           *CountsMPI;             ///< MPI count gathering arrays
   int           *DisplsMPI;             ///< MPI displacement gathering arrays
   int            NbMPI;                 ///< Number of MPI process \todo Figure out why this isn't in #ifdef HAVE_MPI
   int            RankMPI;               ///< Rank of MPI process \todo Figure out how this is different from WorldRank and the why it isn't in #ifdef HAVE_MPI
   int            NbThread;              ///< Number of OpenMP threads
   int            Signal;                ///< Trapped signal (-1: Signal trap disabled)
   TApp_Affinity  Affinity;              ///< Thread placement affinity
   int            NbNodeMPI;
   int            NodeRankMPI;           ///< Number of MPI process on the current node
#ifdef HAVE_MPI
   MPI_Comm       Comm;
   MPI_Comm       NodeComm;              ///< Communicator for the current node
   MPI_Comm       NodeHeadComm;          ///< Communicator for the head nodes

   MPI_Comm       MainComm;              ///< Communicator that groups all executables from this context \todo Figure out if there is any case where this isn't going to be MPI_COMM_WORLD
   int            WorldRank;             ///< Global rank of this PE
   int            ComponentRank;         ///< Local rank of this PE (within its component)
   TComponent *   SelfComponent;         ///< This PE's component
   int            NumComponents;         ///< How many components are part of the MPMD context
   TComponent *   AllComponents;         ///< List of components in this context
   int            NbSets;                ///< How many sets of components are stored in this context
   int            SizeSets;              ///< Size of the array that stores sets of components
   TComponentSet* Sets;                  ///< List of sets that are already stored in this context
#endif //HAVE_MPI

   TApp_Timer     *TimerLog;             ///< Time spent on log printing
} TApp;

#ifndef APP_BUILD
extern __thread TApp *App;               ///< Per thread App pointer

static inline char* App_TimeString(TApp_Timer *Timer,int Total) {
    snprintf(Timer->String,32,"%s%.3f ms%s",(App->LogColor?APP_COLOR_LIGHTGREEN:""),(Total?App_TimerTotalTime_ms(Timer):App_TimerLatestTime_ms(Timer)),(App->LogColor?APP_COLOR_RESET:""));
    return(Timer->String);
}
#endif

typedef int (TApp_InputParseProc) (void *Def, char *Token, char *Value, int Index);

//! Alias to the \ref Lib_Log function with \ref APP_MAIN implicitly provided as first argument
#define App_Log(LEVEL, ...) Lib_Log(APP_MAIN, LEVEL, __VA_ARGS__)

TApp *App_Init(const int Type, const char * const Name, const char * const Version, const char * const Desc, const char * const Stamp);
TApp* App_GetInstance(void);
void  App_LibRegister(const TApp_Lib Lib, const char * const Version);
void  App_Free(void);
void  App_Start(void);
int   App_End(int Status);
int   App_Stats(const char * const Tag);
void Lib_Log(
    const TApp_Lib lib,
    const TApp_LogLevel level,
    const char * const format,
    ...
);
int   Lib_LogLevel(const TApp_Lib Lib, const char * const Val);
int   Lib_LogLevelNo(TApp_Lib Lib, TApp_LogLevel Val);
void  App_LogStream(const char * const Stream);
int   App_LogLevel(const char * const Level);
int   App_LogLevelNo(const TApp_LogLevel Level);
int   App_ToleranceLevel(const char * const Level);
int   App_ToleranceNo(TApp_LogLevel Val);
void  App_LogOpen(void);
void  App_LogClose(void);
int   App_LogTime(const char * const Val);
int   App_LogRank(const int NewRank);
void  App_Progress(const float Percent, const char * const Format, ...);
int   App_ParseArgs(TApp_Arg *AArgs, int argc, char *argv[], int Flags);
int   App_ParseInput(void *Def, char *File, TApp_InputParseProc *ParseProc);
int   App_ParseBool(char *Param, char *Value, char *Var);
int   App_ParseDate(char *Param, char *Value, time_t *Var);
int App_ParseDateSplit(
    const char * const param,
    char * const value,
    int * const year,
    int * const month,
    int * const day,
    int * const hour,
    int * const min);
int App_ParseCoords(
    const char * const Param,
    const char * const Value,
    double * const Lat,
    double * const Lon,
    const int Index);
void  App_SeedInit(void);
char* App_ErrorGet(void);
int   App_ThreadPlace(void);
void  App_Trap(const int Signal);
int   App_IsDone(void); 
int   App_IsMPI(void);
int   App_IsOMP(void);
int   App_IsSingleNode(void);
int   App_IsAloneNode(void);
int   App_NodeGroup();
int   App_NodePrint();

#ifdef HAVE_MPI
void  App_SetMPIComm(MPI_Comm Comm);
int   App_MPIProcCmp(const void *a, const void *b);

#include "App_MPMD.h"
#endif

#endif
